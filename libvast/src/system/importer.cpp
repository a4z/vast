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

#include "vast/system/importer.hpp"

#include "vast/fwd.hpp"

#include "vast/concept/printable/numeric/integral.hpp"
#include "vast/concept/printable/std/chrono.hpp"
#include "vast/concept/printable/to_string.hpp"
#include "vast/concept/printable/vast/error.hpp"
#include "vast/concept/printable/vast/filesystem.hpp"
#include "vast/defaults.hpp"
#include "vast/detail/fill_status_map.hpp"
#include "vast/logger.hpp"
#include "vast/plugin.hpp"
#include "vast/si_literals.hpp"
#include "vast/system/flush_listener_actor.hpp"
#include "vast/system/report.hpp"
#include "vast/system/type_registry_actor.hpp"
#include "vast/table_slice.hpp"

#include <caf/atom.hpp>
#include <caf/attach_continuous_stream_stage.hpp>
#include <caf/config_value.hpp>
#include <caf/settings.hpp>

#include <fstream>

namespace vast::system {

namespace {

class driver : public caf::stream_stage_driver<
                 table_slice, caf::broadcast_downstream_manager<table_slice>> {
public:
  driver(caf::broadcast_downstream_manager<table_slice>& out,
         importer_state& state)
    : stream_stage_driver(out), state{state} {
    // nop
  }

  void process(caf::downstream<table_slice>& out,
               std::vector<table_slice>& slices) override {
    VAST_TRACE(VAST_ARG(slices));
    uint64_t events = 0;
    auto t = timer::start(state.measurement_);
    for (auto&& slice : std::exchange(slices, {})) {
      VAST_ASSERT(slice.rows() <= static_cast<size_t>(state.available_ids()));
      auto rows = slice.rows();
      events += rows;
      slice.offset(state.next_id(rows));
      out.push(std::move(slice));
    }
    t.stop(events);
  }

  void finalize(const error& err) override {
    VAST_DEBUG(state.self, "stopped with message:", render(err));
  }

  importer_state& state;
};

class stream_stage : public caf::detail::stream_stage_impl<driver> {
public:
  /// Constructs the import stream stage.
  /// @note This must explictly initialize the stream_manager because it does
  /// not provide a default constructor, and for reason unbeknownst to me the
  /// forwaring in the stream_stage_impl does not suffice.
  stream_stage(importer_actor* self)
    : stream_manager(self), stream_stage_impl(self, self->state) {
    // nop
  }

  void register_input_path(caf::inbound_path* ptr) override {
    driver_.state.inbound_descriptions[ptr]
      = std::exchange(driver_.state.inbound_description, "anonymous");
    VAST_INFO(driver_.state.self, "adds",
              driver_.state.inbound_descriptions[ptr], "source");
    super::register_input_path(ptr);
  }

  void deregister_input_path(caf::inbound_path* ptr) noexcept override {
    VAST_INFO(driver_.state.self, "removes",
              driver_.state.inbound_descriptions[ptr], "source");
    driver_.state.inbound_descriptions.erase(ptr);
    super::deregister_input_path(ptr);
  }
};

caf::intrusive_ptr<stream_stage> make_importer_stage(importer_actor* self) {
  auto result = caf::make_counted<stream_stage>(self);
  result->continuous(true);
  return result;
}

} // namespace

importer_state::importer_state(caf::event_based_actor* self_ptr)
  : self(self_ptr) {
  // nop
}

importer_state::~importer_state() {
  write_state(write_mode::with_next);
}

caf::error importer_state::read_state() {
  auto file = dir / "current_id_block";
  if (exists(file)) {
    VAST_VERBOSE(self, "reads persistent state from", file);
    std::ifstream state_file{to_string(file)};
    state_file >> current.end;
    if (!state_file)
      return make_error(ec::parse_error, "unable to read importer state file",
                        file.str());
    state_file >> current.next;
    if (!state_file) {
      VAST_WARNING(self, "did not find next ID position in state file; "
                         "irregular shutdown detected");
      current.next = current.end;
    }
  } else {
    VAST_VERBOSE(self, "did not find a state file at", file);
    current.end = 0;
    current.next = 0;
  }
  return get_next_block();
}

caf::error importer_state::write_state(write_mode mode) {
  if (!exists(dir)) {
    if (auto err = mkdir(dir))
      return err;
  }
  std::ofstream state_file{to_string(dir / "current_id_block")};
  state_file << current.end;
  if (mode == write_mode::with_next) {
    state_file << " " << current.next;
    VAST_VERBOSE(self, "persisted next available ID at", current.next);
  } else {
    VAST_VERBOSE(self, "persisted ID block boundary at", current.end);
  }
  return caf::none;
}

caf::error importer_state::get_next_block(uint64_t required) {
  using namespace si_literals;
  while (current.next + required >= current.end)
    current.end += 8_Mi;
  return write_state(write_mode::without_next);
}

id importer_state::next_id(uint64_t advance) {
  id pre = current.next;
  id post = pre + advance;
  if (post >= current.end)
    get_next_block(advance);
  current.next = post;
  VAST_ASSERT(current.next < current.end);
  return pre;
}

id importer_state::available_ids() const noexcept {
  return max_id - current.next;
}

caf::dictionary<caf::config_value>
importer_state::status(status_verbosity v) const {
  auto result = caf::settings{};
  auto& importer_status = put_dictionary(result, "importer");
  // TODO: caf::config_value can only represent signed 64 bit integers, which
  // may make it look like overflow happened in the status report. As an
  // intermediate workaround, we convert the values to strings.
  if (v >= status_verbosity::detailed) {
    caf::put(importer_status, "ids.available", to_string(available_ids()));
    caf::put(importer_status, "ids.block.next", to_string(current.next));
    caf::put(importer_status, "ids.block.end", to_string(current.end));
    auto& sources_status = put_list(importer_status, "sources");
    for (const auto& kv : inbound_descriptions)
      sources_status.emplace_back(kv.second);
  }
  // General state such as open streams.
  if (v >= status_verbosity::debug)
    detail::fill_status_map(importer_status, self);
  return result;
}

void importer_state::send_report() {
  auto now = stopwatch::now();
  if (measurement_.events > 0) {
    using namespace std::string_literals;
    auto elapsed = std::chrono::duration_cast<duration>(now - last_report);
    auto node_throughput = measurement{elapsed, measurement_.events};
    auto r = performance_report{
      {{"importer"s, measurement_}, {"node_throughput"s, node_throughput}}};
#if VAST_LOG_LEVEL >= VAST_LOG_LEVEL_VERBOSE
    auto beat = [&](const auto& sample) {
      if (auto rate = sample.value.rate_per_sec(); std::isfinite(rate))
        VAST_VERBOSE(self, "handled", sample.value.events,
                     "events at a rate of", static_cast<uint64_t>(rate),
                     "events/sec in", to_string(sample.value.duration));
      else
        VAST_VERBOSE(self, "handled", sample.value.events, "events in",
                     to_string(sample.value.duration));
    };
    beat(r[1]);
#endif
    measurement_ = measurement{};
    self->send(accountant, std::move(r));
  }
  last_report = now;
}

caf::behavior importer(importer_actor* self, path dir, archive_actor archive,
                       index_actor index, type_registry_actor type_registry) {
  VAST_TRACE(VAST_ARG(dir));
  self->state.dir = dir;
  auto err = self->state.read_state();
  if (err) {
    VAST_ERROR(self, "failed to load state:", self->system().render(err));
    self->quit(std::move(err));
    return {};
  }
  namespace defs = defaults::system;
  self->set_exit_handler([=](const caf::exit_msg& msg) {
    self->state.send_report();
    self->quit(msg.reason);
  });
  self->state.stage = make_importer_stage(self);
  if (type_registry)
    self->state.stage->add_outbound_path(type_registry);
  if (archive)
    self->state.stage->add_outbound_path(archive);
  if (index) {
    self->state.index = index;
    self->state.stage->add_outbound_path(index);
  }
  for (auto& plugin : plugins::get())
    if (auto p = plugin.as<analyzer_plugin>())
      if (auto analyzer = p->make_analyzer(self->system()))
        self->state.stage->add_outbound_path(analyzer);
  return {
    [=](accountant_actor accountant) {
      VAST_DEBUG(self, "registers accountant", archive);
      self->state.accountant = std::move(accountant);
      self->send(self->state.accountant, atom::announce_v, self->name());
    },
    [=](atom::exporter, const caf::actor& exporter) {
      VAST_DEBUG(self, "registers exporter", exporter);
      return self->state.stage->add_outbound_path(exporter);
    },
    [=](caf::stream<table_slice> in) {
      VAST_DEBUG(self, "adds a new source:", self->current_sender());
      self->state.stage->add_inbound_path(in);
    },
    [=](caf::stream<table_slice> in, std::string desc) {
      VAST_DEBUG(self, "adds a new source:", self->current_sender());
      self->state.inbound_description = std::move(desc);
      self->state.stage->add_inbound_path(in);
    },
    [=](atom::add, const caf::actor& subscriber) {
      auto& st = self->state;
      VAST_DEBUG(self, "adds a new sink:", self->current_sender());
      st.stage->add_outbound_path(subscriber);
    },
    [=](atom::subscribe, atom::flush, wrapped_flush_listener listener) {
      VAST_ASSERT(self->state.stage != nullptr);
      self->send(index, atom::subscribe_v, atom::flush_v, std::move(listener));
    },
    [=](atom::status, status_verbosity v) { return self->state.status(v); },
    [=](atom::telemetry) {
      self->state.send_report();
      self->delayed_send(self, defs::telemetry_rate, atom::telemetry_v);
    },
  };
}

} // namespace vast::system
