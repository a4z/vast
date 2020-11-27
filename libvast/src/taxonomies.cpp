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

#include "vast/taxonomies.hpp"

#include "vast/concept/printable/vast/data.hpp"
#include "vast/detail/stable_set.hpp"
#include "vast/error.hpp"
#include "vast/expression.hpp"
#include "vast/logger.hpp"

#include <caf/deserializer.hpp>
#include <caf/serializer.hpp>

#include <algorithm>
#include <deque>
#include <stack>
#include <string_view>

namespace vast {

caf::error convert(const data& d, concepts_map& out) {
  const auto& c = caf::get_if<record>(&d);
  if (!c)
    return make_error(ec::convert_error, "concept is not a record:", d);
  auto name_data = c->find("name");
  if (name_data == c->end())
    return make_error(ec::convert_error, "concept has no name:", d);
  auto name = caf::get_if<std::string>(&name_data->second);
  if (!name)
    return make_error(ec::convert_error,
                      "concept name is not a string:", *name_data);
  auto& dest = out[*name];
  auto fs = c->find("fields");
  if (fs != c->end()) {
    const auto& fields = caf::get_if<list>(&fs->second);
    if (!fields)
      return make_error(ec::convert_error, "fields in", *name,
                        "is not a list:", fs->second);
    for (auto& f : *fields) {
      auto field = caf::get_if<std::string>(&f);
      if (!field)
        return make_error(ec::convert_error, "field in", *name,
                          "is not a string:", f);
      if (std::count(dest.fields.begin(), dest.fields.end(), *field) > 0)
        VAST_WARNING_ANON("ignoring duplicate field for",
                          *name + ": \"" + *field + "\"");
      else
        dest.fields.push_back(*field);
    }
  }
  auto cs = c->find("concepts");
  if (cs != c->end()) {
    const auto& concepts = caf::get_if<list>(&cs->second);
    if (!concepts)
      return make_error(ec::convert_error, "concepts in", *name,
                        "is not a list:", cs->second);
    for (auto& c : *concepts) {
      auto concept_ = caf::get_if<std::string>(&c);
      if (!concept_)
        return make_error(ec::convert_error, "concept in", *name,
                          "is not a string:", c);
      if (std::count(dest.fields.begin(), dest.fields.end(), *concept_) > 0)
        VAST_WARNING_ANON("ignoring duplicate concept for",
                          *name + ": \"" + *concept_ + "\"");
      else
        dest.fields.push_back(*concept_);
    }
  }
  auto desc = c->find("description");
  if (desc != c->end()) {
    if (auto description = caf::get_if<std::string>(&desc->second)) {
      if (dest.description.empty())
        dest.description = *description;
      else if (dest.description != *description)
        VAST_WARNING_ANON("encountered conflicting descriptions for",
                          *name + ": \"" + dest.description + "\" and \""
                            + *description + "\"");
    }
  }
  return caf::none;
}

caf::error extract_concepts(const data& d, concepts_map& out) {
  if (const auto& xs = caf::get_if<list>(&d)) {
    for (const auto& item : *xs) {
      if (const auto& x = caf::get_if<record>(&item)) {
        auto n = x->find("concept");
        if (n == x->end())
          continue;
        if (auto err = convert(n->second, out))
          return err;
      }
    }
  }
  return caf::none;
}

caf::expected<concepts_map> extract_concepts(const data& d) {
  concepts_map result;
  if (auto err = extract_concepts(d, result))
    return err;
  return result;
}

bool operator==(const concept_& lhs, const concept_& rhs) {
  return lhs.concepts == rhs.concepts && lhs.fields == rhs.fields;
}

caf::error convert(const data& d, models_map& out) {
  const auto& c = caf::get_if<record>(&d);
  if (!c)
    return make_error(ec::convert_error, "concept is not a record:", d);
  auto name_data = c->find("name");
  if (name_data == c->end())
    return make_error(ec::convert_error, "concept has no name:", d);
  auto name = caf::get_if<std::string>(&name_data->second);
  if (!name)
    return make_error(ec::convert_error,
                      "concept name is not a string:", *name_data);
  if (out.find(*name) != out.end())
    return make_error(ec::convert_error,
                      "models cannot have multiple definitions", *name);
  auto& dest = out[*name];
  auto def = c->find("definition");
  if (def != c->end()) {
    const auto& def_list = caf::get_if<list>(&def->second);
    if (!def_list)
      return make_error(ec::convert_error, "definition in", *name,
                        "is not a list:", def->second);
    for (auto& x : *def_list) {
      auto component = caf::get_if<std::string>(&x);
      if (!component)
        return make_error(ec::convert_error, "component in", *name,
                          "is not a string:", x);
      dest.definition.push_back(*component);
    }
  }
  auto desc = c->find("description");
  if (desc != c->end()) {
    if (auto description = caf::get_if<std::string>(&desc->second)) {
      if (dest.description.empty())
        dest.description = *description;
      else if (dest.description != *description)
        VAST_WARNING_ANON("encountered conflicting descriptions for",
                          *name + ": \"" + dest.description + "\" and \""
                            + *description + "\"");
    }
  }
  return caf::none;
}

caf::error extract_models(const data& d, models_map& out) {
  if (const auto& xs = caf::get_if<list>(&d)) {
    for (const auto& item : *xs) {
      if (const auto& x = caf::get_if<record>(&item)) {
        auto n = x->find("model");
        if (n == x->end())
          continue;
        if (auto err = convert(n->second, out))
          return err;
      }
    }
  }
  return caf::none;
}

caf::expected<models_map> extract_models(const data& d) {
  models_map result;
  if (auto err = extract_models(d, result))
    return err;
  return result;
}

bool operator==(const model& lhs, const model& rhs) {
  return lhs.definition == rhs.definition;
}

bool operator==(const taxonomies& lhs, const taxonomies& rhs) {
  return lhs.concepts == rhs.concepts && lhs.models == rhs.models;
}

static bool
contains(const std::map<std::string, type_set>& seen, const std::string& x,
         relational_operator op, const vast::data& data) {
  std::string::size_type pos = 0;
  while ((pos = x.find('.', pos)) != std::string::npos) {
    auto i = seen.find(std::string{std::string_view{x.c_str(), pos}});
    if (i != seen.end()) {
      // A prefix of x matches an existing layout.
      auto field = x.substr(pos + 1);
      return std::any_of(i->second.value.begin(), i->second.value.end(),
                         [&](const type& t) {
                           if (auto r = caf::get_if<record_type>(&t)) {
                             if (auto f = r->find(field))
                               return compatible(f->type, op, data);
                           }
                           return false;
                         });
    }
    ++pos;
  }
  return false;
}

static caf::expected<expression>
resolve_impl(const taxonomies& ts, const expression& e,
             const std::map<std::string, type_set>& seen, bool prune) {
  return for_each_predicate(e, [&](const auto& pred) -> caf::expected<expression> {
    // TODO: Rename appropriately.
    auto resolve_concepts = [&](const std::string& field_name,
                                relational_operator op, const vast::data& data,
                                auto make_predicate) {
      // This algorithm recursivly looks up items form the concepts map and
      // generates a predicate for every discovered name that is not a concept
      // itself.
      auto c = ts.concepts.find(field_name);
      if (c == ts.concepts.end())
        return expression{std::move(pred)};
      // The log of all referenced concepts that we tried to resolve already.
      // This is a deque instead of a stable_set because we don't want
      // push_back to invalidate the `current` iterator.
      std::deque<std::string> log;
      // All fields that the concept resolves to either directly or indirectly
      // through referenced concepts.
      detail::stable_set<std::string> target_fields;
      auto load_definition = [&](const concept_& def) {
        // Create the union of all fields by inserting into the set.
        target_fields.insert(def.fields.begin(), def.fields.end());
        // Insert only those concepts into the queue that aren't in there yet,
        // this prevents infinite loops through circular references between
        // concepts.
        for (auto& x : def.concepts)
          if (std::find(log.begin(), log.end(), x) == log.end())
            log.push_back(x);
      };
      load_definition(c->second);
      // We iterate through the log while appending referenced concepts in
      // load_definition.
      for (auto current : log)
        if (auto ref = ts.concepts.find(current); ref != ts.concepts.end())
          load_definition(ref->second);
      // Transform the target_fields into new predicates.
      disjunction d;
      auto make_pred = make_predicate(data);
      for (auto& x : target_fields) {
        if (!prune || contains(seen, x, op, data))
          d.emplace_back(make_pred(std::move(x)));
      }
      switch (d.size()) {
        case 0:
          return expression{std::move(pred)};
        case 1:
          return d[0];
        default:
          return expression{d};
      }
    };
    auto resolve_models
      = [&](const std::string& field_name, relational_operator op,
            const vast::data& data,
            auto make_predicate) -> caf::expected<expression> {
      auto r = caf::get_if<record>(&data);
      if (!r)
        // Models can only be compared to records, so if the data side is
        // not a record, we move to the concept substitution phase directly.
        return resolve_concepts(field_name, op, data, make_predicate);
      if (r->empty())
        return expression{caf::none};
      auto it = ts.models.find(field_name);
      if (it == ts.models.end())
        return resolve_concepts(field_name, op, data, make_predicate);
      // We have a model predicate.
      conjunction c;
      auto named = !r->begin()->first.empty();
      if (named) {
        // TODO: Nested records of the form
        // <src_endpoint: <1.2.3.4, _>, process_filename: "svchost.exe">
        // are currently not supported.
        for (auto& x : *r) {
          VAST_ASSERT(!x.first.empty());
          // TODO: Check that x.first is contained in it->second and return an
          // error if not.
          c.emplace_back(
            resolve_concepts(x.first, op, x.second, make_predicate));
        }
        if (c.empty())
          return make_error(ec::invalid_query, "empty record queries are not "
                                               "supported yet");
      } else {
        // TODO: Explain the tree iteration mechanism.
        auto p = std::pair{it->second.definition.begin(),
                           it->second.definition.end()};
        auto levels = std::stack{std::vector{std::move(p)}};
        for (auto value_iterator = r->begin(); value_iterator != r->end();
             ++value_iterator, ++levels.top().first) {
          VAST_ASSERT(value_iterator->first.empty());
          {
            // Update the levels stack; explicit scope for clarity.
            while (levels.top().first == levels.top().second) {
              levels.pop();
              if (levels.empty())
                // The provided record is longer then the matched concept.
                // TODO: This error could be rendered in a way that makes it
                // clear how the mismatch happened. For example:
                //   src_ip, src_port,  dst_ip, dst_port, proto
                // <      _,        _, 1.2.3.4,        _,     _, "tcp">
                //                                               ^~~~~
                //                                               too many fields
                //                                               provided
                return make_error(ec::invalid_query, *r,
                                  "doesn't match the model:", it->first);
              ++levels.top().first;
            }
            for (auto child_component = ts.models.find(*levels.top().first);
                 child_component != ts.models.end();
                 child_component = ts.models.find(*levels.top().first)) {
              auto& child_def = child_component->second.definition;
              levels.emplace(child_def.begin(), child_def.end());
            }
            // Empty models ought to be rejected at load time.
            VAST_ASSERT(levels.top().first != levels.top().second);
          }
          if (caf::holds_alternative<caf::none_t>(value_iterator->second))
            continue;
          c.emplace_back(resolve_concepts(
            *levels.top().first, op, value_iterator->second, make_predicate));
        }
        if (c.empty())
          return make_error(ec::invalid_query, "placeholder only queries are "
                                               "not supported yet");
        // TODO: If the provided record is shorter than the model the missing
        // fields are treated as if they are '_'. Consider treating this as
        // an error.
      }
      VAST_ASSERT(!c.empty());
      switch (c.size()) {
        case 1:
          return c[0];
        default:
          return expression{c};
      }
    };
    if (auto fe = caf::get_if<field_extractor>(&pred.lhs)) {
      if (auto data = caf::get_if<vast::data>(&pred.rhs)) {
        return resolve_models(
          fe->field, pred.op, *data, [&](const vast::data& o) {
            return [&](const std::string& item) {
              return predicate{field_extractor{item}, pred.op, o};
            };
          });
      }
    }
    if (auto fe = caf::get_if<field_extractor>(&pred.rhs)) {
      if (auto data = caf::get_if<vast::data>(&pred.lhs)) {
        return resolve_models(
          fe->field, flip(pred.op), *data, [&](const vast::data& o) {
            return [&](const std::string& item) {
              return predicate{o, pred.op, field_extractor{item}};
            };
          });
      }
    }
    return expression{pred};
  });
}

caf::expected<expression> resolve(const taxonomies& ts, const expression& e) {
  return resolve_impl(ts, e, {}, false);
}

caf::expected<expression> resolve(const taxonomies& ts, const expression& e,
                                  const std::map<std::string, type_set>& seen) {
  return resolve_impl(ts, e, seen, true);
}

} // namespace vast
