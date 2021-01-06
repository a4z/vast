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
//#include "vast/logger_backward.hpp"
#include "vast/config.hpp"
#include "vast/defaults.hpp"
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

caf::detail::scope_guard<void (*)()>
create_log_context(const vast::system::configuration& cfg) {
  vast::detail::setup_spdlog(cfg);
  return caf::detail::make_scope_guard(
    std::addressof(vast::detail::shutdown_spdlog));
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

// TODO could be like that ^^ but I think it's good to have the async_logger
// here since it also documents the real type
// std::shared_ptr<spdlog::logger> vast_logger =
// spdlog::null_logger_mt("/dev/null");

} // namespace

namespace detail {

bool setup_spdlog(const system::configuration& cfg) {
  if (vast_logger && vast_logger->name() != "/dev/null") {
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
      return spdlog::color_mode::automatic;
    if (config_value == "always")
      return spdlog::color_mode::always;

    return spdlog::color_mode::never;
  }();

  auto logfile_in_cfg = caf::get_if<std::string>(&cfg, "vast.log-file");
  std::string logfile = logfile_in_cfg
                          ? *logfile_in_cfg
                          : caf::get_or(cfg, "logger.file-name", "server.log");

  spdlog::init_thread_pool(8192, 1);

  auto stderr_sink
    = std::make_shared<spdlog::sinks::ansicolor_stderr_sink_mt>(log_color);
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

void shutdown_spdlog() {
  VAST_LOG_SPD_DEBUG("shut down logging") ;

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

} // namespace vast

namespace vast::backwards {

line_builder& line_builder::operator<<(const caf::local_actor* self) {
  if (not self)
    return *this << "NULL Pointer";

  return *this << self->name();
}

line_builder& line_builder::operator<<(const std::string& str) {
  return *this << str.c_str();
}

line_builder& line_builder::operator<<(caf::string_view str) {
  if (!str_.empty() && str_.back() != ' ')
    str_ += " ";
  str_.insert(str_.end(), str.begin(), str.end());
  return *this;
}

line_builder& line_builder::operator<<(const char* str) {
  if (!str_.empty() && str_.back() != ' ')
    str_ += " ";
  str_ += str;
  return *this;
}

line_builder& line_builder::operator<<(char x) {
  const char buf[] = {x, '\0'};
  return *this << buf;
}

const std::string& line_builder::get() const {
  return str_;
}

} // namespace vast::backwards
