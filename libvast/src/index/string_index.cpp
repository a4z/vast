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

#include "vast/index/string_index.hpp"

#include "vast/bitmap_algorithms.hpp"
#include "vast/defaults.hpp"
#include "vast/detail/overload.hpp"
#include "vast/index/container_lookup.hpp"
#include "vast/type.hpp"

#include <caf/deserializer.hpp>
#include <caf/serializer.hpp>
#include <caf/settings.hpp>

namespace vast {

string_index::string_index(vast::type t, caf::settings opts)
  : value_index{std::move(t), std::move(opts)} {
  max_length_
    = caf::get_or(options(), "max-size", defaults::index::max_string_size);
  auto b = base::uniform(10, std::log10(max_length_) + !!(max_length_ % 10));
  length_ = length_bitmap_index{std::move(b)};
}

caf::error string_index::serialize(caf::serializer& sink) const {
  return caf::error::eval([&] { return value_index::serialize(sink); },
                          [&] { return sink(max_length_, length_, chars_); });
}

caf::error string_index::deserialize(caf::deserializer& source) {
  return caf::error::eval([&] { return value_index::deserialize(source); },
                          [&] { return source(max_length_, length_, chars_); });
}

bool string_index::append_impl(data_view x, id pos) {
  auto str = caf::get_if<view<std::string>>(&x);
  if (!str)
    return false;
  auto length = str->size();
  if (length > max_length_)
    length = max_length_;
  if (length > chars_.size())
    chars_.resize(length, char_bitmap_index{8});
  for (auto i = 0u; i < length; ++i) {
    chars_[i].skip(pos - chars_[i].size());
    chars_[i].append(static_cast<uint8_t>((*str)[i]));
  }
  length_.skip(pos - length_.size());
  length_.append(length);
  return true;
}

caf::expected<ids>
string_index::lookup_impl(relational_operator op, data_view x) const {
  auto f = detail::overload{
    [&](auto x) -> caf::expected<ids> {
      return caf::make_error(ec::type_clash, materialize(x));
    },
    [&](view<std::string> str) -> caf::expected<ids> {
      auto str_size = str.size();
      if (str_size > max_length_)
        str_size = max_length_;
      switch (op) {
        default:
          return caf::make_error(ec::unsupported_operator, op);
        case equal:
        case not_equal: {
          if (str_size == 0) {
            auto result = length_.lookup(equal, 0);
            if (op == not_equal)
              result.flip();
            return result;
          }
          if (str_size > chars_.size())
            return ids{offset(), op == not_equal};
          auto result = length_.lookup(less_equal, str_size);
          if (all<0>(result))
            return ids{offset(), op == not_equal};
          for (auto i = 0u; i < str_size; ++i) {
            auto b = chars_[i].lookup(equal, static_cast<uint8_t>(str[i]));
            result &= b;
            if (all<0>(result))
              return ids{offset(), op == not_equal};
          }
          if (op == not_equal)
            result.flip();
          return result;
        }
        case ni:
        case not_ni: {
          if (str_size == 0)
            return ids{offset(), op == ni};
          if (str_size > chars_.size())
            return ids{offset(), op == not_ni};
          // TODO: Be more clever than iterating over all k-grams (#45).
          ids result{offset(), false};
          for (auto i = 0u; i < chars_.size() - str_size + 1; ++i) {
            ids substr{offset(), true};
            auto skip = false;
            for (auto j = 0u; j < str_size; ++j) {
              auto bm = chars_[i + j].lookup(equal, str[j]);
              if (all<0>(bm)) {
                skip = true;
                break;
              }
              substr &= bm;
            }
            if (!skip)
              result |= substr;
          }
          if (op == not_ni)
            result.flip();
          return result;
        }
      }
    },
    [&](view<list> xs) { return detail::container_lookup(*this, op, xs); },
  };
  return caf::visit(f, x);
}

size_t string_index::memusage_impl() const {
  size_t acc = length_.memusage();
  for (const auto& char_index : chars_)
    acc += char_index.memusage();
  return acc;
}

} // namespace vast
