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

#include "vast/logger.hpp"

#include "vast/config.hpp"
#include "vast/defaults.hpp"
#include "vast/error.hpp"
#include "vast/system/configuration.hpp"

#include <caf/atom.hpp>
#include <caf/local_actor.hpp>

#include <spdlog/async.h>
#include <spdlog/common.h>
#include <spdlog/sinks/rotating_file_sink.h>
//#include <spdlog/sinks/stdout_color_sinks.h>
#include <cassert>
#include <memory>

#include <spdlog/sinks/ansicolor_sink.h>
#include <spdlog/sinks/null_sink.h>

// TODO remove
#include <iostream>

namespace vast {

caf::optional<caf::detail::scope_guard<void (*)()>>
create_log_context(const vast::invocation& cmd_invocation,
                   const caf::settings& cfg_file) {
  if (!vast::detail::setup_spdlog(cmd_invocation, cfg_file))
    return {};

  return {caf::detail::make_scope_guard(
    std::addressof(vast::detail::shutdown_spdlog))};
}

namespace {

constexpr bool is_vast_loglevel(const int value) {
  switch (value) {
    case VAST_LOG_LEVEL_QUIET:
    case VAST_LOG_LEVEL_ERROR:
    case VAST_LOG_LEVEL_WARNING:
    case VAST_LOG_LEVEL_INFO:
    case VAST_LOG_LEVEL_VERBOSE:
    case VAST_LOG_LEVEL_DEBUG:
    case VAST_LOG_LEVEL_TRACE:
      return true;
  }
  return false;
}

// (base on info from chat)
/// Converts a vast log level to spdlog level
constexpr spdlog::level::level_enum vast_loglevel_to_spd(const int value) {
  assert(is_vast_loglevel(value));

  spdlog::level::level_enum level = spdlog::level::off;
  switch (value) {
    case VAST_LOG_LEVEL_QUIET:
      break;
    case VAST_LOG_LEVEL_ERROR:
      level = spdlog::level::err;
      break;
    case VAST_LOG_LEVEL_WARNING:
      level = spdlog::level::warn;
      break;
    case VAST_LOG_LEVEL_INFO:
      level = spdlog::level::info;
      break;
    case VAST_LOG_LEVEL_VERBOSE:
      level = spdlog::level::debug;
      break;
    case VAST_LOG_LEVEL_DEBUG:
      level = spdlog::level::debug;
      break;
    case VAST_LOG_LEVEL_TRACE:
      level = spdlog::level::trace;
      break;
  }
  return level;
}

// there are log calls done before the log was created ,
// so something to ignore is needed
inline std::shared_ptr<spdlog::async_logger> dev_null_logger() {
  auto null_logger
    = spdlog::async_factory::template create<spdlog::sinks::null_sink_mt>(
      "/dev/null");
  null_logger->set_level(spdlog::level::off);
  return null_logger;
}

std::shared_ptr<spdlog::async_logger> vast_logger = dev_null_logger();

} // namespace

namespace detail {

bool setup_spdlog(const vast::invocation& cmd_invocation,
                  const caf::settings& cfg_file) {
  // already running .....
  if (vast_logger && vast_logger->name() != "/dev/null") {
    VAST_ERROR_ANON("Log already up");
    return false;
  }



  bool is_server = cmd_invocation.name() == "start"
                   || caf::get_or(cmd_invocation.options, "vast.node", false);

  const auto& cfg_cmd = cmd_invocation.options;

  auto verbosity = caf::get_if<caf::atom_value>(&cfg_cmd, "vast.verbosity");

  auto file_verbosity = verbosity ? *verbosity : vast::defaults::logger::file_verbosity;
  auto console_verbosity = verbosity ? *verbosity : vast::defaults::logger::console_verbosity;
  file_verbosity = caf::get_or(cfg_file, "vast.file-verbosity", file_verbosity);
  console_verbosity
    = caf::get_or(cfg_file, "vast.console-verbosity", console_verbosity);

  auto cfg_file_verbosity = loglevel_to_int(file_verbosity);
  auto cfg_console_verbosity = loglevel_to_int(console_verbosity);
  auto cfg_verbosity = std::max(cfg_file_verbosity, cfg_console_verbosity);

  // There can maybesomething be done with atom ? (TODO check improve)
  spdlog::color_mode log_color = [&]() -> spdlog::color_mode {
    auto config_value = caf::get_or(cfg_file, "vast.console", "automatic");
    if (config_value == "automatic")
      return spdlog::color_mode::automatic;
    if (config_value == "always")
      return spdlog::color_mode::always;

    return spdlog::color_mode::never;
  }();

  auto log_file =caf::get_or(cfg_file, "vast.log-file", std::string{defaults::logger::log_file});

  auto cmdline_log_file = caf::get_if<std::string>(&cfg_cmd, "vast.log-file");
  if (cmdline_log_file){
    log_file = *cmdline_log_file ;
  }

  if (is_server) {
    path log_dir = caf::get_or(cfg_file, "vast.db-directory",
                                defaults::system::db_directory);
    if (!exists(log_dir)) {
      if (auto err = mkdir(log_dir)) {
        // TODO make an on demand spd console log ?
        std::cerr << "unable to create directory: " << log_dir.str() << ' '
                  << vast::render(err) << '\n';
        return false;
      }
      log_file = (log_dir / log_file).str();
    }
  } else {
    // please note, client file does not go to db_directory, wanted ?
    //first command line, then config file
    auto client_log_file = caf::get_if<std::string>(&cfg_cmd, "vast.client-log-file");
    if(!client_log_file)
      client_log_file = caf::get_if<std::string>(&cfg_file, "vast.client-log-file");
    if (client_log_file)
      log_file = *client_log_file ;
    else // if there is no client log file, turn off file logging
      cfg_file_verbosity = VAST_LOG_LEVEL_QUIET;
  }


  spdlog::init_thread_pool(8192, 1);

  std::vector<spdlog::sink_ptr> sinks;

  auto stderr_sink
    = std::make_shared<spdlog::sinks::ansicolor_stderr_sink_mt>(log_color);
  stderr_sink->set_level(vast_loglevel_to_spd(cfg_console_verbosity));
  auto console_format = caf::get_if<std::string>(&cfg_file, "vast.console-format");
  if (console_format){
    stderr_sink->set_pattern(*console_format) ;
  }

  sinks.push_back(stderr_sink);

  if (cfg_file_verbosity != VAST_LOG_LEVEL_QUIET) {
    // TODO , rodate, not rodate, ...
    auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
      log_file, 1024 * 1024 * 10, 3);
    file_sink->set_level(vast_loglevel_to_spd(cfg_file_verbosity));
    auto file_format = caf::get_if<std::string>(&cfg_file, "vast.file-format");
    if (file_format){
      stderr_sink->set_pattern(*file_format) ;
    }
    sinks.push_back(file_sink);
  }



  vast_logger = std::make_shared<spdlog::async_logger>(
    "vast", sinks.begin(), sinks.end(), spdlog::thread_pool(),
    spdlog::async_overflow_policy::block);

  vast_logger->set_level(vast_loglevel_to_spd(cfg_verbosity));

  spdlog::register_logger(vast_logger);

  return true;
}

void shutdown_spdlog() {
  VAST_LOG_SPD_DEBUG("shut down logging");

  spdlog::shutdown();
}

std::shared_ptr<spdlog::logger> logger() {
  return vast_logger;
}

} // namespace detail

} // namespace vast

// ------------

namespace vast {

namespace {

// A trick that uses explicit template instantiation of a static member
// as explained here: https://gist.github.com/dabrahams/1528856
template <class Tag>
struct stowed {
  static typename Tag::type value;
};
template <class Tag>
typename Tag::type stowed<Tag>::value;

template <class Tag, typename Tag::type x>
struct stow_private {
  stow_private() {
    stowed<Tag>::value = x;
  }
  static stow_private instance;
};
template <class Tag, typename Tag::type x>
stow_private<Tag, x> stow_private<Tag, x>::instance;

// A tag type for caf::logger::cfg.
struct logger_cfg {
  using type = caf::logger::config(caf::logger::*);
};

template struct stow_private<logger_cfg, &caf::logger::cfg_>;

} // namespace

int loglevel_to_int(caf::atom_value x, int default_value) {
  switch (caf::atom_uint(to_lowercase(x))) {
    case caf::atom_uint("quiet"):
      return VAST_LOG_LEVEL_QUIET;
    case caf::atom_uint("error"):
      return VAST_LOG_LEVEL_ERROR;
    case caf::atom_uint("warning"):
      return VAST_LOG_LEVEL_WARNING;
    case caf::atom_uint("info"):
      return VAST_LOG_LEVEL_INFO;
    case caf::atom_uint("verbose"):
      return VAST_LOG_LEVEL_VERBOSE;
    case caf::atom_uint("debug"):
      return VAST_LOG_LEVEL_DEBUG;
    case caf::atom_uint("trace"):
      return VAST_LOG_LEVEL_TRACE;
    default:
      return default_value;
  }
}

} // namespace vast
