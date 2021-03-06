/******************************************************************************
 *                    _   _____   __________                                  *
 *                   | | / / _ | / __/_  __/     Visibility                   *
 *                   | |/ / __ |_\ \  / /          Across                     *
 *                   |___/_/ |_/___/ /_/       Space and Time                 *
 *                                                                            *
 * This file is part of VAST. It is subject to the license terms in the       *
 * LICENSE file found in the top-level directory of this distribution and at  *
 * http://vast.io/license. No part of VAST, including this file, may be       *
 * copied, modified, propagated, or distributed except according to the terms *
 * contained in the LICENSE file.                                             *
 ******************************************************************************/

#include "vast/meta_index.hpp"

#include "vast/fwd.hpp"

#include "vast/data.hpp"
#include "vast/detail/overload.hpp"
#include "vast/detail/set_operations.hpp"
#include "vast/detail/string.hpp"
#include "vast/detail/tracepoint.hpp"
#include "vast/expression.hpp"
#include "vast/fbs/utils.hpp"
#include "vast/logger.hpp"
#include "vast/synopsis.hpp"
#include "vast/synopsis_factory.hpp"
#include "vast/system/instrumentation.hpp"
#include "vast/table_slice.hpp"
#include "vast/time.hpp"

#include <caf/binary_deserializer.hpp>
#include <caf/binary_serializer.hpp>

#include <type_traits>

namespace vast {

void partition_synopsis::shrink() {
  for (auto& [field, synopsis] : field_synopses_) {
    if (!synopsis)
      continue;
    auto shrinked_synopsis = synopsis->shrink();
    if (!shrinked_synopsis)
      continue;
    synopsis.swap(shrinked_synopsis);
  }
  // TODO: Make a utility function instead of copy/pasting
  for (auto& [field, synopsis] : type_synopses_) {
    if (!synopsis)
      continue;
    auto shrinked_synopsis = synopsis->shrink();
    if (!shrinked_synopsis)
      continue;
    synopsis.swap(shrinked_synopsis);
  }
}

void partition_synopsis::add(const table_slice& slice,
                             const caf::settings& synopsis_options) {
  auto make_synopsis = [&](const type& t) -> synopsis_ptr {
    return has_skip_attribute(t) ? nullptr
                                 : factory<synopsis>::make(t, synopsis_options);
  };
  auto& layout = slice.layout();
  auto each = record_type::each(layout);
  auto field_it = each.begin();
  for (size_t col = 0; col < slice.columns(); ++col, ++field_it) {
    auto& type = field_it->type();
    auto add_column = [&](const synopsis_ptr& syn) {
      for (size_t row = 0; row < slice.rows(); ++row) {
        auto view = slice.at(row, col, type);
        if (!caf::holds_alternative<caf::none_t>(view))
          syn->add(std::move(view));
      }
    };
    auto key = qualified_record_field{layout.name(), *field_it};
    if (!caf::holds_alternative<string_type>(type)) {
      // Locate the relevant synopsis.
      auto it = field_synopses_.find(key);
      if (it == field_synopses_.end()) {
        // Attempt to create a synopsis if we have never seen this key before.
        it = field_synopses_.emplace(std::move(key), make_synopsis(type)).first;
      }
      // If there exists a synopsis for a field, add the entire column.
      if (auto& syn = it->second)
        add_column(syn);
    } else { // type == string
      field_synopses_[key] = nullptr;
      auto cleaned_type = vast::type{field_it->type()}.attributes({});
      auto tt = type_synopses_.find(cleaned_type);
      if (tt == type_synopses_.end())
        tt = type_synopses_.emplace(cleaned_type, make_synopsis(type)).first;
      if (auto& syn = tt->second)
        add_column(syn);
    }
  }
}

size_t partition_synopsis::size_bytes() const {
  size_t result = 0;
  for (auto& [field, synopsis] : field_synopses_)
    result += synopsis ? synopsis->size_bytes() : 0ull;
  return result;
}

size_t meta_index::size_bytes() const {
  size_t result = 0;
  for (auto& [id, partition_synopsis] : synopses_)
    result += partition_synopsis.size_bytes();
  return result;
}

void meta_index::add(const uuid& partition, const table_slice& slice) {
  auto& part_syn = synopses_[partition];
  part_syn.add(slice, synopsis_options_);
}

void meta_index::erase(const uuid& partition) {
  synopses_.erase(partition);
}

void meta_index::merge(const uuid& partition, partition_synopsis&& ps) {
  synopses_[partition] = std::move(ps);
}

partition_synopsis& meta_index::at(const uuid& partition) {
  return synopses_.at(partition);
}

void meta_index::replace(const uuid& partition,
                         std::unique_ptr<partition_synopsis> ps) {
  auto it = synopses_.find(partition);
  if (it != synopses_.end()) {
    it->second.field_synopses_.swap(ps->field_synopses_);
  }
}

std::vector<uuid> meta_index::lookup(const expression& expr) const {
  VAST_ASSERT(!caf::holds_alternative<caf::none_t>(expr));
  auto start = system::stopwatch::now();
  // TODO: we could consider a flat_set<uuid> here, which would then have
  // overloads for inplace intersection/union and simplify the implementation
  // of this function a bit. This would also simplify the maintainance of a
  // critical invariant: partition UUIDs must be sorted. Otherwise the
  // invariants of the inplace union and intersection algorithms are violated,
  // leading to wrong results. This invariant is easily violated because we
  // currently just append results to the candidate vector, so all places where
  // we return an assembled set must ensure the post-condition of returning a
  // sorted list.
  using result_type = std::vector<uuid>;
  result_type memoized_partitions;
  auto all_partitions = [&] {
    if (!memoized_partitions.empty() || synopses_.empty())
      return memoized_partitions;
    memoized_partitions.reserve(synopses_.size());
    std::transform(synopses_.begin(), synopses_.end(),
                   std::back_inserter(memoized_partitions),
                   [](auto& x) { return x.first; });
    std::sort(memoized_partitions.begin(), memoized_partitions.end());
    return memoized_partitions;
  };
  auto f = detail::overload{
    [&](const conjunction& x) -> result_type {
      VAST_ASSERT(!x.empty());
      auto i = x.begin();
      auto result = lookup(*i);
      if (!result.empty())
        for (++i; i != x.end(); ++i) {
          auto xs = lookup(*i);
          if (xs.empty())
            return xs; // short-circuit
          detail::inplace_intersect(result, xs);
          VAST_ASSERT(std::is_sorted(result.begin(), result.end()));
        }
      return result;
    },
    [&](const disjunction& x) -> result_type {
      result_type result;
      for (auto& op : x) {
        auto xs = lookup(op);
        VAST_ASSERT(std::is_sorted(xs.begin(), xs.end()));
        if (xs.size() == synopses_.size())
          return xs; // short-circuit
        detail::inplace_unify(result, xs);
        VAST_ASSERT(std::is_sorted(result.begin(), result.end()));
      }
      return result;
    },
    [&](const negation&) -> result_type {
      // We cannot handle negations, because a synopsis may return false
      // positives, and negating such a result may cause false
      // negatives.
      // TODO: The above statement seems to only apply to bloom filter
      // synopses, but it should be possible to handle time or bool synopses.
      return all_partitions();
    },
    [&](const predicate& x) -> result_type {
      // Performs a lookup on all *matching* synopses with operator and
      // data from the predicate of the expression. The match function
      // uses a qualified_record_field to determine whether the synopsis should
      // be queried.
      auto search = [&](auto match) {
        VAST_ASSERT(caf::holds_alternative<data>(x.rhs));
        auto& rhs = caf::get<data>(x.rhs);
        result_type result;
        for (auto& [part_id, part_syn] : synopses_) {
          for (auto& [field, syn] : part_syn.field_synopses_) {
            if (match(field)) {
              auto cleaned_type = vast::type{field.type}.attributes({});
              // We rely on having a field -> nullptr mapping here for the
              // fields that don't have their own synopsis.
              if (syn) {
                auto opt = syn->lookup(x.op, make_view(rhs));
                if (!opt || *opt) {
                  VAST_DEBUG(this, "selects", part_id, "at predicate", x);
                  result.push_back(part_id);
                  break;
                }
                // The field has no dedicated synopsis. Check if there is one
                // for the type in general.
              } else if (auto it = part_syn.type_synopses_.find(cleaned_type);
                         it != part_syn.type_synopses_.end() && it->second) {
                auto opt = it->second->lookup(x.op, make_view(rhs));
                if (!opt || *opt) {
                  VAST_DEBUG(this, "selects", part_id, "at predicate", x);
                  result.push_back(part_id);
                  break;
                }
              } else {
                // The meta index couldn't rule out this partition, so we have
                // to include it in the result set.
                result.push_back(part_id);
                break;
              }
            }
          }
        }
        VAST_DEBUG(this, "checked", synopses_.size(),
                   "partitions for predicate", x, "and got", result.size(),
                   "results");
        // Some calling paths require the result to be sorted.
        std::sort(result.begin(), result.end());
        return result;
      };
      auto extract_expr = detail::overload{
        [&](const attribute_extractor& lhs, const data& d) -> result_type {
          if (lhs.attr == atom::timestamp_v) {
            auto pred = [](auto& field) {
              return has_attribute(field.type, "timestamp");
            };
            return search(pred);
          } else if (lhs.attr == atom::type_v) {
            // We don't have to look into the synopses for type queries, just
            // at the layout names.
            result_type result;
            for (auto& [part_id, part_syn] : synopses_) {
              for (auto& pair : part_syn.field_synopses_) {
                // TODO: provide an overload for view of evaluate() so that
                // we can use string_view here. Fortunately type names are
                // short, so we're probably not hitting the allocator due to
                // SSO.
                auto type_name = data{pair.first.layout_name};
                if (evaluate(type_name, x.op, d)) {
                  result.push_back(part_id);
                  break;
                }
              }
            }
            // Re-establish potentially violated invariant.
            std::sort(result.begin(), result.end());
            return result;
          } else if (lhs.attr == atom::field_v) {
            // We don't have to look into the synopses for type queries, just
            // at the layout names.
            result_type result;
            auto s = caf::get_if<std::string>(&d);
            if (!s) {
              VAST_WARNING_ANON("#field meta queries only support string "
                                "comparisons");
            } else {
              for (const auto& synopsis : synopses_) {
                // Compare the desired field name with each field in the
                // partition.
                auto matching = [&] {
                  for (const auto& pair : synopsis.second.field_synopses_) {
                    auto fqn = pair.first.fqn();
                    if (detail::ends_with(fqn, *s))
                      return true;
                  }
                  return false;
                }();
                // Only insert the partition if both sides are equal, i.e. the
                // operator is "positive" and matching is true, or both are
                // negative.
                if (!is_negated(x.op) == matching)
                  result.push_back(synopsis.first);
              }
            }
            // Re-establish potentially violated invariant.
            std::sort(result.begin(), result.end());
            return result;
          }
          VAST_WARNING(this, "cannot process attribute extractor:", lhs.attr);
          return all_partitions();
        },
        [&](const field_extractor& lhs, const data&) -> result_type {
          auto pred = [&](auto& field) {
            return detail::ends_with(field.fqn(), lhs.field);
          };
          return search(pred);
        },
        [&](const type_extractor& lhs, const data&) -> result_type {
          auto pred = [&](auto& field) { return field.type == lhs.type; };
          return search(pred);
        },
        [&](const auto&, const auto&) -> result_type {
          VAST_WARNING(this, "cannot process predicate:", x);
          return all_partitions();
        },
      };
      return caf::visit(extract_expr, x.lhs, x.rhs);
    },
    [&](caf::none_t) -> result_type {
      VAST_ERROR(this, "received an empty expression");
      VAST_ASSERT(!"invalid expression");
      return all_partitions();
    },
  };
  auto result = caf::visit(f, expr);
  auto delta = std::chrono::duration_cast<std::chrono::microseconds>(
    system::stopwatch::now() - start);
  VAST_DEBUG_ANON("meta index lookup found", result.size(), "candidates in",
                  delta.count(), "microseconds");
  VAST_TRACEPOINT(meta_index_lookup, delta.count(), result.size());
  return result;
}

caf::settings& meta_index::factory_options() {
  return synopsis_options_;
}

caf::expected<flatbuffers::Offset<fbs::partition_synopsis::v0>>
pack(flatbuffers::FlatBufferBuilder& builder, const partition_synopsis& x) {
  std::vector<flatbuffers::Offset<fbs::synopsis::v0>> synopses;
  for (auto& [fqf, synopsis] : x.field_synopses_) {
    auto maybe_synopsis = pack(builder, synopsis, fqf);
    if (!maybe_synopsis)
      return maybe_synopsis.error();
    synopses.push_back(*maybe_synopsis);
  }
  for (auto& [type, synopsis] : x.type_synopses_) {
    qualified_record_field fqf;
    fqf.type = type;
    auto maybe_synopsis = pack(builder, synopsis, fqf);
    if (!maybe_synopsis)
      return maybe_synopsis.error();
    synopses.push_back(*maybe_synopsis);
  }
  auto synopses_vector = builder.CreateVector(synopses);
  fbs::partition_synopsis::v0Builder ps_builder(builder);
  ps_builder.add_synopses(synopses_vector);
  return ps_builder.Finish();
}

caf::error
unpack(const fbs::partition_synopsis::v0& x, partition_synopsis& ps) {
  if (!x.synopses())
    return make_error(ec::format_error, "missing synopses");
  for (auto synopsis : *x.synopses()) {
    if (!synopsis)
      return make_error(ec::format_error, "synopsis is null");
    qualified_record_field qf;
    if (auto error
        = fbs::deserialize_bytes(synopsis->qualified_record_field(), qf))
      return error;
    synopsis_ptr ptr;
    if (auto error = unpack(*synopsis, ptr))
      return error;
    if (!qf.field_name.empty())
      ps.field_synopses_[qf] = std::move(ptr);
    else
      ps.type_synopses_[qf.type] = std::move(ptr);
  }
  return caf::none;
}

} // namespace vast
