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
#include "vast/system/configuration.hpp"

#include <caf/atom.hpp>


#include <spdlog/async.h>
#include <spdlog/common.h>
#include <spdlog/sinks/rotating_file_sink.h>
//#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/ansicolor_sink.h>


#include <memory>
#include <cassert>

// TODO remove
#include <iostream>

namespace vast {

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

std::shared_ptr<spdlog::async_logger> vast_logger = nullptr;

} // namespace

bool setup_spdlog(const system::configuration& cfg)
{
  if (vast_logger) {
    return false;
  }
  namespace lg = defaults::logger;
  auto verbosity = caf::get_if<caf::atom_value>(&cfg, "vast.verbosity");
  auto file_verbosity = verbosity ? *verbosity : lg::file_verbosity;
  auto console_verbosity = verbosity ? *verbosity : lg::console_verbosity;
  file_verbosity = get_or(cfg, "vast.file-verbosity", file_verbosity);
  console_verbosity
    = caf::get_or(cfg, "vast.console-verbosity", console_verbosity);
  auto vast_file_verbosity = loglevel_to_int(file_verbosity);
  auto vast_console_verbosity = loglevel_to_int(console_verbosity);
  auto vast_verbosity = std::max(vast_file_verbosity, vast_console_verbosity);

  // There can maybesomething be done with atom ? (TODO check improve)
  spdlog::color_mode log_color = [&]() -> spdlog::color_mode {
    auto config_value = caf::get_or(cfg, "vast.console", "automatic");
    if (config_value == "automatic")
      return spdlog::color_mode::automatic ;
    if (config_value == "always")
      return spdlog::color_mode::always ;

    return spdlog::color_mode::never ;
  } () ;

  auto logfile_in_cfg = caf::get_if<std::string>(&cfg, "vast.log-file");
  std::string logfile = logfile_in_cfg ?
    *logfile_in_cfg : caf::get_or(cfg, "logger.file-name", "server.log");


  spdlog::init_thread_pool(8192, 1);

  auto stderr_sink =
     std::make_shared<spdlog::sinks::ansicolor_stderr_sink_mt>(log_color);
  // TODO , rodate, not rodate, ...
  auto rotating_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
    logfile, 1024 * 1024 * 10, 3);
  std::vector<spdlog::sink_ptr> sinks{stderr_sink, rotating_sink};

  vast_logger = std::make_shared<spdlog::async_logger>(
    "vast", sinks.begin(), sinks.end(), spdlog::thread_pool(),
    spdlog::async_overflow_policy::block);

  vast_logger->set_level(vast_loglevel_to_spd(vast_verbosity));
  stderr_sink->set_level(vast_loglevel_to_spd(vast_console_verbosity));
  rotating_sink->set_level(vast_loglevel_to_spd(vast_file_verbosity));

  spdlog::register_logger(vast_logger);

  return true;
}

std::shared_ptr<spdlog::logger> log() {
  return vast_logger;
}

}

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
    default:
      return default_value;
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
  }
}

void fixup_logger(const system::configuration& cfg) {
  setup_spdlog(cfg) ;
  // Reset the logger so we can support the VERBOSE level
  namespace lg = defaults::logger;
  auto logger = caf::logger::current_logger();
  // A workaround for the lack of an accessor function for logger.cfg_,
  // see https://github.com/actor-framework/actor-framework/issues/1066.
  auto& cfg_ = logger->*stowed<logger_cfg>::value;
  auto verbosity = caf::get_if<caf::atom_value>(&cfg, "vast.verbosity");
  auto file_verbosity = verbosity ? *verbosity : lg::file_verbosity;
  auto console_verbosity = verbosity ? *verbosity : lg::console_verbosity;
  file_verbosity = get_or(cfg, "vast.file-verbosity", file_verbosity);
  console_verbosity
    = caf::get_or(cfg, "vast.console-verbosity", console_verbosity);
  cfg_.file_verbosity = loglevel_to_int(file_verbosity);
  cfg_.console_verbosity = loglevel_to_int(console_verbosity);
  cfg_.verbosity = std::max(cfg_.file_verbosity, cfg_.console_verbosity);
}

} // namespace vast
