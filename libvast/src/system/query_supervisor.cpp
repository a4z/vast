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

#include "vast/system/query_supervisor.hpp"

#include "vast/fwd.hpp"

#include "vast/expression.hpp"
#include "vast/logger.hpp"
#include "vast/system/index_client_actor.hpp"
#include "vast/system/query_supervisor_master_actor.hpp"

#include <algorithm>

namespace vast::system {

namespace {

[[maybe_unused]] auto get_ids(const query_map& xs) {
  std::vector<uuid> ys;
  ys.reserve(xs.size());
  std::transform(xs.begin(), xs.end(), std::back_inserter(ys),
                 [](auto& kvp) { return kvp.first; });
  return ys;
}

} // namespace

query_supervisor_state::query_supervisor_state(
  query_supervisor_actor::stateful_pointer<query_supervisor_state> self)
  : open_requests(0), name("query_supervisor-") {
  name += std::to_string(self->id());
}

query_supervisor_actor::behavior_type query_supervisor(
  query_supervisor_actor::stateful_pointer<query_supervisor_state> self,
  query_supervisor_master_actor master) {
  // Ask master for initial work.
  self->send(master, atom::worker_v, self);
  return {
    [=](const expression& expr,
        const std::vector<std::pair<uuid, partition_actor>>& qm,
        const index_client_actor& client) {
      VAST_DEBUG(self, "got a new query for", qm.size(),
                 "partitions:", get_ids(qm));
      VAST_ASSERT(!qm.empty());
      VAST_ASSERT(self->state.open_requests == 0);
      for (auto& [id, partition] : qm) {
        ++self->state.open_requests;
        // TODO: Add a proper configurable timeout.
        self
          ->request(partition, caf::infinite, expr,
                    static_cast<const partition_client_actor&>(client))
          .then(
            [=](atom::done) {
              if (--self->state.open_requests == 0) {
                VAST_DEBUG(self, "collected all results for the current batch "
                                 "of partitions");
                // Ask master for more work after receiving the last sub
                // result.
                // TODO: We should schedule a new partition as soon as the
                // prvious one has finished, otherwise each batch will be as
                // slow as the worst case of the batch.
                self->send(client, atom::done_v);
                self->send(master, atom::worker_v, self);
              }
            },
            [=](const caf::error& e) {
              // TODO: Add a proper error handling path to escalate the error to
              // the client.
              VAST_ERROR(self, "encountered error while supervising query", e);
              if (--self->state.open_requests == 0) {
                self->send(client, atom::done_v);
                self->send(master, atom::worker_v, self);
              }
            });
      }
    },
  };
}

} // namespace vast::system
