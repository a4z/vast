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

#pragma once

#include "vast/synopsis.hpp"

namespace vast {

// A synopsis for a [bool type](@ref bool_type).
class bool_synopsis : public synopsis {
public:
  explicit bool_synopsis(vast::type x);

  bool_synopsis(bool true_, bool false_);

  void add(data_view x) override;

  caf::optional<bool> lookup(relational_operator op,
                             data_view rhs) const override;

  bool equals(const synopsis& other) const noexcept override;

  size_t memusage() const override;

  caf::error serialize(caf::serializer& sink) const override;

  caf::error deserialize(caf::deserializer& source) override;

  bool any_true();

  bool any_false();

private:
  bool true_ = false;
  bool false_ = false;
};

} // namespace vast
