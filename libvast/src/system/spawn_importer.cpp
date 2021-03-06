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

#include "vast/system/spawn_importer.hpp"

#include "vast/defaults.hpp"
#include "vast/detail/assert.hpp"
#include "vast/logger.hpp"
#include "vast/system/archive_actor.hpp"
#include "vast/system/importer.hpp"
#include "vast/system/index_actor.hpp"
#include "vast/system/node.hpp"
#include "vast/system/spawn_arguments.hpp"

namespace vast::system {

maybe_actor spawn_importer(node_actor* self, spawn_arguments& args) {
  if (!args.empty())
    return unexpected_arguments(args);
  // FIXME: Notify exporters with a continuous query.
  auto [archive, index, type_registry]
    = self->state.registry.find_by_label("archive", "index", "type-registry");
  if (!archive)
    return make_error(ec::missing_component, "archive");
  if (!index)
    return make_error(ec::missing_component, "index");
  if (!type_registry)
    return make_error(ec::missing_component, "type-registry");
  auto handle
    = self->spawn(importer, args.dir / args.label,
                  caf::actor_cast<archive_actor>(archive),
                  caf::actor_cast<index_actor>(index),
                  caf::actor_cast<type_registry_actor>(type_registry));
  VAST_VERBOSE(self, "spawned the importer");
  if (auto accountant = self->state.registry.find_by_label("accountant")) {
    self->send(handle, atom::telemetry_v);
    self->send(handle, caf::actor_cast<accountant_actor>(accountant));
  } else if (auto logger = caf::logger::current_logger();
             logger && logger->console_verbosity() >= VAST_LOG_LEVEL_VERBOSE) {
    // Initiate periodic rate logging.
    // TODO: Implement live-reloading of the importer configuration.
    self->send(handle, atom::telemetry_v);
  }
  for (auto& a : self->state.registry.find_by_type("source")) {
    VAST_DEBUG(self, "connects source to new importer");
    self->send(a, atom::sink_v, handle);
  }
  return handle;
}

} // namespace vast::system
