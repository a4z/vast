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

#include "vast/fwd.hpp"

#include "vast/fbs/partition.hpp"
#include "vast/path.hpp"
#include "vast/system/accountant_actor.hpp"
#include "vast/system/active_indexer_actor.hpp"
#include "vast/system/filesystem_actor.hpp"
#include "vast/system/indexer_actor.hpp"
#include "vast/system/instrumentation.hpp"
#include "vast/type.hpp"
#include "vast/uuid.hpp"

#include <string>

namespace vast::system {

// TODO: Create a separate `passive_indexer_state`, similar to how partitions
// are handled.
struct indexer_state {
  /// The name of this indexer.
  std::string name;

  /// The index holding the data.
  value_index_ptr idx;

  /// Whether the type of this indexer has the `#skip` attribute, implying that
  /// the incoming data should not be indexed.
  bool has_skip_attribute;

  /// The partition id to which this indexer belongs (for log messages).
  uuid partition_id;

  /// Tracks whether we received at least one table slice column.
  bool stream_initiated;

  /// The response promise for a snapshot atom.
  caf::typed_response_promise<chunk_ptr> promise;
};

/// Indexes a table slice column with a single value index.
active_indexer_actor::behavior_type
active_indexer(active_indexer_actor::stateful_pointer<indexer_state> self,
               type index_type, caf::settings index_opts);

/// An indexer that was recovered from on-disk state. It can only respond
/// to queries, but not add eny more entries.
indexer_actor::behavior_type
passive_indexer(indexer_actor::stateful_pointer<indexer_state> self,
                uuid partition_id, value_index_ptr idx);

} // namespace vast::system
