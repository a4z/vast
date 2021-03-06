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

#include "vast/system/component_registry.hpp"

#include "vast/detail/assert.hpp"

#include <algorithm>

namespace vast::system {

bool component_registry::add(caf::actor comp, std::string type,
                             std::string label) {
  VAST_ASSERT(comp);
  if (label.empty())
    label = type;
#if VAST_ENABLE_ASSERTIONS
  auto pred = [&](auto& x) { return x.second.actor == comp; };
  VAST_ASSERT(std::none_of(components_.begin(), components_.end(), pred));
#endif
  return components_
    .emplace(std::move(label), component{std::move(comp), std::move(type)})
    .second;
}

bool component_registry::remove(const caf::actor& comp) {
  for (auto i = components_.begin(); i != components_.end(); ++i) {
    if (i->second.actor == comp) {
      components_.erase(i);
      return true;
    }
  }
  return false;
}

const std::string*
component_registry::find_label_for(const caf::actor& comp) const {
  auto pred = [&](auto& x) { return x.second.actor == comp; };
  auto i = std::find_if(components_.begin(), components_.end(), pred);
  return i != components_.end() ? &i->first : nullptr;
}

const std::string*
component_registry::find_type_for(const caf::actor& comp) const {
  auto pred = [&](auto& x) { return x.second.actor == comp; };
  auto i = std::find_if(components_.begin(), components_.end(), pred);
  return i != components_.end() ? &i->second.type : nullptr;
}

caf::actor component_registry::find_by_label(std::string_view label) const {
  // TODO: remove string conversion in with C++20 transparent keys.
  if (auto i = components_.find(std::string{label}); i != components_.end())
    return i->second.actor;
  return {};
}

std::vector<caf::actor>
component_registry::find_by_type(std::string_view type) const {
  auto result = std::vector<caf::actor>{};
  for ([[maybe_unused]] auto& [label, comp] : components_)
    if (comp.type == type)
      result.push_back(comp.actor);
  return result;
}

const component_registry::component_map&
component_registry::components() const {
  return components_;
}

} // namespace vast::system
