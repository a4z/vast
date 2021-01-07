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

#include "vast/config.hpp"
#include "vast/detail/discard.hpp"
#include "vast/detail/pp.hpp"
#include "vast/detail/type_traits.hpp"
//#include "vast/logger_backwards.hpp" // compatible ;-)

#include <caf/detail/pretty_type_name.hpp>
#include <caf/detail/scope_guard.hpp>

// from chat .. TODO, verify
// VAST_INFO -> spdlog::info
// VAST_VERBOSE -> spdlog::debug
// VAST_DEBUG -> spdlog::trace
// VAST_TRACE -> spdlog::trace

#if VAST_LOG_LEVEL == VAST_LOG_LEVEL_TRACE
#  define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#elif VAST_LOG_LEVEL == VAST_LOG_LEVEL_DEBUG
#  define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#elif VAST_LOG_LEVEL == VAST_LOG_LEVEL_VERBOSE
#  define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_DEBUG
#elif VAST_LOG_LEVEL == VAST_LOG_LEVEL_INFO
#  define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_INFO
#elif VAST_LOG_LEVEL == VAST_LOG_LEVEL_WARNING
#  define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_WARN
#elif VAST_LOG_LEVEL == VAST_LOG_LEVEL_ERROR
#  define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_ERROR
#elif VAST_LOG_LEVEL == VAST_LOG_LEVEL_CRITICAL
#  define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_CRITICAL
#elif VAST_LOG_LEVEL == VAST_LOG_LEVEL_QUIET
#  define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_OFF
#endif

// TODO this is deprectated,
// use detail instead of internal, but old spdlog in Debian ...
// #define FMT_USE_INTERNAL 1  // does not work ???
// #include <spdlog/fmt/ostr.h>
// #include <spdlog/spdlog.h>
// keep that extra for now, to see which includes are required
#include "loggerformatters.hpp"


#define VAST_LOG_SPD_TRACE(...)                                                \
  SPDLOG_LOGGER_TRACE(vast::detail::logger(), __VA_ARGS__)
#define VAST_LOG_SPD_DEBUG(...)                                                \
  SPDLOG_LOGGER_DEBUG(vast::detail::logger(), __VA_ARGS__)
#define VAST_LOG_SPD_VERBOSE(...)                                              \
  SPDLOG_LOGGER_DEBUG(vast::detail::logger(), __VA_ARGS__)
#define VAST_LOG_SPD_INFO(...)                                                 \
  SPDLOG_LOGGER_INFO(vast::detail::logger(), __VA_ARGS__)
#define VAST_LOG_SPD_WARN(...)                                                 \
  SPDLOG_LOGGER_WARN(vast::detail::logger(), __VA_ARGS__)
#define VAST_LOG_SPD_ERROR(...)                                                \
  SPDLOG_LOGGER_ERROR(vast::detail::logger(), __VA_ARGS__)
#define VAST_LOG_SPD_CRITICAL(...)                                             \
  SPDLOG_LOGGER_CRITICAL(vast::detail::logger(), __VA_ARGS__)

// -------

namespace vast {

namespace system {

class configuration;

}

namespace detail {
/// Initialize the spdlog
/// Creates the log and the sinks, sets loglevels and format
/// Must be called before using the logger, otherwise log messages will
/// silently be discarded.
bool setup_spdlog(const system::configuration& cfg);

/// Shuts down the logging system
/// Since vast logger runs async and has therefore a  background thread.
/// for a graceful exit this function should be called.
/// TODO THERE SHOULD BE A LOG CONTEXT (GUARD) BUT WITHOUT DESIGN DISCUSSON I
/// WILL NOT IMPLEMENT THAT
void shutdown_spdlog();

/// Get a spdlog::logger handel
std::shared_ptr<spdlog::logger> logger();

} // namespace detail

/// Converts a verbosity atom to its integer counterpart. For unknown atoms,
/// the `default_value` parameter will be returned.
/// Used to make log level strings from config, like 'debug', to a log level int
int loglevel_to_int(caf::atom_value ll, int default_value
                                        = VAST_LOG_LEVEL_QUIET);

[[nodiscard]] caf::detail::scope_guard<void (*)()>
create_log_context(const vast::system::configuration& cfg);

} // namespace vast

// -- VAST logging macros ------------------------------------------------------

namespace vast::detail {

template <class T>
auto id_or_name(T&& x) {
  static_assert(!std::is_same_v<const char*, std::remove_reference<T>>,
                "const char* is not allowed for the first argument in a "
                "logging statement. Supply a component or use "
                "VAST_[ERROR|WARNING|INFO|VERBOSE|DEBUG]_ANON instead.");
  if constexpr (std::is_pointer_v<T>) {
    using value_type = std::remove_pointer_t<T>;
    if constexpr (has_ostream_operator<value_type>)
      return *x;
    else if constexpr (has_to_string<value_type>)
      return to_string(*x);
    else
      return caf::detail::pretty_type_name(typeid(value_type));
  } else {
    if constexpr (has_ostream_operator<T>)
      return std::forward<T>(x);
    else if constexpr (has_to_string<T>)
      return to_string(std::forward<T>(x));
    else
      return caf::detail::pretty_type_name(typeid(T));
  }
}

template <typename... T>
vast::backwards::line_builder mk_line_builder(T&&... ) {

  vast::backwards::line_builder lb;
 // (lb << ... << t);
  return lb;
}

template <size_t S>
struct carrier {
  char name[S] = {0};
  constexpr const char* str() const {
    return &name[0];
    ;
  }
};
template <typename... T>
constexpr auto spd_msg_from_args(T&&...) {
  constexpr auto cnt = sizeof...(T);
  static_assert(cnt > 0);
  constexpr auto len = cnt * 3;
  carrier<len> cr{};
  for (size_t i = 0; i < cnt; ++i) {
    cr.name[i * 3] = '{';
    cr.name[i * 3 + 1] = '}';
    cr.name[i * 3 + 2] = ' ';
  }
  cr.name[len - 1] = 0;
  return cr;
}

} // namespace vast::detail





#if defined(VAST_LOG_LEVEL)

#  if VAST_LOG_LEVEL >= VAST_LOG_LEVEL_TRACE
// #    define VAST_TRACE(...)                                                    \
//       SPDLOG_LOGGER_TRACE(vast::detail::logger(), "ENTER {} {}", __func__,     \
//                           vast::detail::mk_line_builder(__VA_ARGS__));         \
//       auto CAF_UNIFYN(vast_log_trace_guard_)                                   \
//         = ::caf::detail::make_scope_guard([=, func_name_ = __func__] {         \
//             SPDLOG_LOGGER_TRACE(vast::detail::logger(), "ENTER {}",            \
//                                 func_name_);                                   \
//           })


#    define VAST_TRACE(...)                                                    \
      SPDLOG_LOGGER_TRACE(vast::detail::logger(),                              \
                      vast::detail::spd_msg_from_args(1, 2, __VA_ARGS__).str(),\
                          "ENTER",  __func__, __VA_ARGS__);                  \
      auto CAF_UNIFYN(vast_log_trace_guard_)                                   \
        = ::caf::detail::make_scope_guard([=, func_name_ = __func__] {         \
            SPDLOG_LOGGER_TRACE(vast::detail::logger(), "ENTER {}",            \
                                func_name_);                                   \
          })

#  else // VAST_LOG_LEVEL > VAST_LOG_LEVEL_TRACE

#    define VAST_TRACE(...) VAST_DISCARD_ARGS(__VA_ARGS__)

#  endif // VAST_LOG_LEVEL > VAST_LOG_LEVEL_TRACE

#  define VAST_ERROR(c, ...)                                                   \
    SPDLOG_LOGGER_DEBUG(vast::detail::logger(),                                \
                        vast::detail::spd_msg_from_args(c, __VA_ARGS__).str(), \
                        ::vast::detail::id_or_name(c), __VA_ARGS__)

#  define VAST_ERROR_ANON(...)                                                   \
    SPDLOG_LOGGER_DEBUG(vast::detail::logger(),                                \
                        vast::detail::spd_msg_from_args(__VA_ARGS__).str(), \
                         __VA_ARGS__)


#  define VAST_WARNING(c, ...)                                                 \
    SPDLOG_LOGGER_WARN(vast::detail::logger(),                                 \
                        vast::detail::spd_msg_from_args(c, __VA_ARGS__).str(), \
                        ::vast::detail::id_or_name(c), __VA_ARGS__)

#  define VAST_WARNING_ANON(...)                                               \
    SPDLOG_LOGGER_WARN(vast::detail::logger(),                                \
                        vast::detail::spd_msg_from_args(__VA_ARGS__).str(), \
                         __VA_ARGS__)


#  define VAST_INFO(c, ...)                                                    \
    SPDLOG_LOGGER_INFO(vast::detail::logger(),                            \
                        vast::detail::spd_msg_from_args(c, __VA_ARGS__).str(), \
                        ::vast::detail::id_or_name(c), __VA_ARGS__)

#  define VAST_INFO_ANON(...)                                                  \
    SPDLOG_LOGGER_INFO(vast::detail::logger(),                            \
                        vast::detail::spd_msg_from_args(__VA_ARGS__).str(), \
                         __VA_ARGS__)

#  define VAST_VERBOSE(c, ...)                                                 \
    SPDLOG_LOGGER_DEBUG(vast::detail::logger(),                           \
                        vast::detail::spd_msg_from_args(c, __VA_ARGS__).str(), \
                        ::vast::detail::id_or_name(c), __VA_ARGS__)

#  define VAST_VERBOSE_ANON(...)                                               \
    SPDLOG_LOGGER_DEBUG(vast::detail::logger(),                           \
                        vast::detail::spd_msg_from_args(__VA_ARGS__).str(), \
                         __VA_ARGS__)


#  define VAST_DEBUG(c, ...)                                                   \
    SPDLOG_LOGGER_TRACE(vast::detail::logger(),                           \
                        vast::detail::spd_msg_from_args(c, __VA_ARGS__).str(), \
                        ::vast::detail::id_or_name(c), __VA_ARGS__)

#  define VAST_DEBUG_ANON(...)                                                 \
    SPDLOG_LOGGER_TRACE(vast::detail::logger(),                           \
                        vast::detail::spd_msg_from_args(__VA_ARGS__).str(), \
                         __VA_ARGS__)

#  define VAST_CRITICAL(c, ...)                                                \
    SPDLOG_LOGGER_CRITICAL(vast::detail::logger(),                       \
                        vast::detail::spd_msg_from_args(c, __VA_ARGS__).str(), \
                        ::vast::detail::id_or_name(c), __VA_ARGS__)

#  define VAST_CRITICAL_ANON(...)                                              \
    SPDLOG_LOGGER_CRITICAL(vast::detail::logger(),                       \
                        vast::detail::spd_msg_from_args(__VA_ARGS__).str(), \
                         __VA_ARGS__)
#else // defined(VAST_LOG_LEVEL)

#  define VAST_LOG(...) VAST_DISCARD_ARGS(__VA_ARGS__)

#  define VAST_LOG_COMPONENT(...) VAST_DISCARD_ARGS(__VA_ARGS__)

#  define VAST_TRACE(...) VAST_DISCARD_ARGS(__VA_ARGS__)

#  define VAST_ERROR(...) VAST_DISCARD_ARGS(__VA_ARGS__)
#  define VAST_ERROR_ANON(...) VAST_DISCARD_ARGS(__VA_ARGS__)

#  define VAST_WARNING(...) VAST_DISCARD_ARGS(__VA_ARGS__)
#  define VAST_WARNING_ANON(...) VAST_DISCARD_ARGS(__VA_ARGS__)

#  define VAST_INFO(...) VAST_DISCARD_ARGS(__VA_ARGS__)
#  define VAST_INFO_ANON(...) VAST_DISCARD_ARGS(__VA_ARGS__)

#  define VAST_VERBOSE(...) VAST_DISCARD_ARGS(__VA_ARGS__)
#  define VAST_VERBOSE_ANON(...) VAST_DISCARD_ARGS(__VA_ARGS__)

#  define VAST_DEBUG(...) VAST_DISCARD_ARGS(__VA_ARGS__)
#  define VAST_DEBUG_ANON(...) VAST_DISCARD_ARGS(__VA_ARGS__)

#endif // defined(VAST_LOG_LEVEL)

// -- VAST_ARG utility for formatting log output -------------------------------

// #define VAST_ARG_1(x) CAF_ARG(x)

// #define VAST_ARG_2(x_name, x) CAF_ARG2(x_name, x)

// #define VAST_ARG_3(x_name, first, last) CAF_ARG3(x_name, first, last)

#define VAST_ARG_1(x) vast::backwards::make_arg_wrapper(#x, x)

#define VAST_ARG_2(x_name, x) vast::backwards::make_arg_wrapper(x_name, x)

#define VAST_ARG_3(x_name, first, last)                                        \
  vast::backwards::make_arg_wrapper(x_name, first, last)

/// Nicely formats a variable or argument. For example, `VAST_ARG(foo)`
/// generates `foo = ...` in log output, where `...` is the content of the
/// variable. `VAST_ARG("size", xs.size())` generates the output
/// `size = xs.size()`.
#define VAST_ARG(...) VAST_PP_OVERLOAD(VAST_ARG_, __VA_ARGS__)(__VA_ARGS__)
