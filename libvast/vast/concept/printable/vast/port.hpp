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

#include "vast/concept/printable/core/printer.hpp"
#include "vast/concept/printable/numeric/integral.hpp"
#include "vast/concept/printable/string/char.hpp"
#include "vast/concept/printable/string/string.hpp"
#include "vast/port.hpp"

namespace vast {

struct port_printer : vast::printer<port_printer> {
  using attribute = port;

  template <class Iterator>
  bool print(Iterator& out, const port& p) const {
    using namespace printers;
    if (!(u16(out, p.number()) && chr<'/'>(out)))
      return false;
    switch (p.type()) {
      default:
        return chr<'?'>(out);
      case port_type::icmp:
        return str(out, "icmp");
      case port_type::tcp:
        return str(out, "tcp");
      case port_type::udp:
        return str(out, "udp");
      case port_type::icmp6:
        return str(out, "icmp6");
      case port_type::sctp:
        return str(out, "sctp");
    }
  }
};

template <>
struct printer_registry<port> {
  using type = port_printer;
};

} // namespace vast
