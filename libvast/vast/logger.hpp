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
#include "vast/logger_backwards.hpp" // compatible ;-)

#include <caf/detail/pretty_type_name.hpp>
//#include <caf/logger.hpp>


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
#  define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_INFO  // TOOD should be debug ?
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

#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>

#define VAST_LOG_SPD_TRACE(...) SPDLOG_LOGGER_TRACE(vast::log(), __VA_ARGS__)
#define VAST_LOG_SPD_DEBUG(...) SPDLOG_LOGGER_DEBUG(vast::log(), __VA_ARGS__)
#define VAST_LOG_SPD_VERBOSE(...) SPDLOG_LOGGER_INFO(vast::log(), __VA_ARGS__) // TODO should be debug ?
#define VAST_LOG_SPD_INFO(...) SPDLOG_LOGGER_INFO(vast::log(), __VA_ARGS__)
#define VAST_LOG_SPD_WARN(...) SPDLOG_LOGGER_WARN(vast::log(), __VA_ARGS__)
#define VAST_LOG_SPD_ERROR(...) SPDLOG_LOGGER_ERROR(vast::log(), __VA_ARGS__)
#define VAST_LOG_SPD_CRITICAL(...) SPDLOG_LOGGER_CRITICAL(vast::log(), __VA_ARGS__)

// -------


namespace vast {

namespace system {

class configuration;

}

namespace detail{
  template<std::size_t S> struct carrier {
    char name[S] = {0};
    constexpr const char* str() const {
      return &name[0];;
    }
  };
  template<typename ... T>
  constexpr auto spd_msg_from_args(T && ... )  {
    constexpr auto cnt =  sizeof...(T) ;
    static_assert(cnt > 0) ;
    constexpr auto len = cnt * 3 ;
    carrier<len> cr {} ;
    for (size_t i = 0; i < cnt; ++i) {
      cr.name[i*3] = '{';
      cr.name[i*3 + 1] = '}';
      cr.name[i*3 + 2] = ' ';
    }
    cr.name[len - 1] = 0 ;
    return cr ;
  }
}




bool setup_spdlog(const system::configuration& cfg);
std::shared_ptr<spdlog::logger> log() ;

/// Converts a verbosity atom to its integer counterpart. For unknown atoms,
/// the `default_value` parameter will be returned.
int loglevel_to_int(caf::atom_value ll, int default_value
                                        = VAST_LOG_LEVEL_QUIET);

// void fixup_logger(const system::configuration& cfg);


void log_backwards(int level, const backwards::line_builder& lb) ;

} // namespace vast

// -- VAST logging macros ------------------------------------------------------

#if defined(VAST_LOG_LEVEL)

//#define VAST_LOG_IMPL(lvl, msg) CAF_LOG_IMPL("vast", lvl, msg)
//#define VAST_LOG_IMPL(lvl, msg) do { vast::log_backwards(lvl, vast::backwards::line_builder{} << msg) ; CAF_LOG_IMPL("vast", lvl, msg); } while(false)
#define VAST_LOG_IMPL(lvl, msg) vast::log_backwards(lvl, vast::backwards::line_builder{} << msg)

#define VAST_LOG_2(lvl, m1) VAST_LOG_IMPL(lvl, m1)

#  define VAST_LOG_3(lvl, m1, m2) VAST_LOG_IMPL(lvl, (m1) << (m2))

#  define VAST_LOG_4(lvl, m1, m2, m3) VAST_LOG_IMPL(lvl, (m1) << (m2) << (m3))

#  define VAST_LOG_5(lvl, m1, m2, m3, m4)                                      \
    VAST_LOG_IMPL(lvl, (m1) << (m2) << (m3) << (m4))

#  define VAST_LOG_6(lvl, m1, m2, m3, m4, m5)                                  \
    VAST_LOG_IMPL(lvl, (m1) << (m2) << (m3) << (m4) << (m5))

#  define VAST_LOG_7(lvl, m1, m2, m3, m4, m5, m6)                              \
    VAST_LOG_IMPL(lvl, (m1) << (m2) << (m3) << (m4) << (m5) << (m6))

#  define VAST_LOG_8(lvl, m1, m2, m3, m4, m5, m6, m7)                          \
    VAST_LOG_IMPL(lvl, (m1) << (m2) << (m3) << (m4) << (m5) << (m6) << (m7))

#  define VAST_LOG_9(lvl, m1, m2, m3, m4, m5, m6, m7, m8)                      \
    VAST_LOG_IMPL(lvl, (m1) << (m2) << (m3) << (m4) << (m5) << (m6) << (m7)    \
                            << (m8))

#  define VAST_LOG_10(lvl, m1, m2, m3, m4, m5, m6, m7, m8, m9)                 \
    VAST_LOG_IMPL(lvl, (m1) << (m2) << (m3) << (m4) << (m5) << (m6) << (m7)    \
                            << (m8) << (m9))

#  define VAST_LOG_11(lvl, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10)            \
    VAST_LOG_IMPL(lvl, (m1) << (m2) << (m3) << (m4) << (m5) << (m6) << (m7)    \
                            << (m8) << (m9) << (m10))

#  define VAST_LOG(...) VAST_PP_OVERLOAD(VAST_LOG_, __VA_ARGS__)(__VA_ARGS__)

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

} // namespace vast::detail

#define VAST_LOG_COMPONENT(lvl, m, ...)                                        \
  VAST_LOG(lvl, ::vast::detail::id_or_name(m), __VA_ARGS__)

#  if VAST_LOG_LEVEL >= VAST_LOG_LEVEL_TRACE

#    define VAST_TRACE(...)                                                    \
      VAST_LOG(VAST_LOG_LEVEL_TRACE, "ENTER", __func__, __VA_ARGS__);          \
      auto CAF_UNIFYN(vast_log_trace_guard_)                                   \
        = ::caf::detail::make_scope_guard([=, func_name_ = __func__] {         \
            VAST_LOG(VAST_LOG_LEVEL_TRACE, "EXIT", func_name_);                \
          })

#  else // VAST_LOG_LEVEL > VAST_LOG_LEVEL_TRACE

#    define VAST_TRACE(...) VAST_DISCARD_ARGS(__VA_ARGS__)

#  endif // VAST_LOG_LEVEL > VAST_LOG_LEVEL_TRACE

#define VAST_ERROR(...) SPDLOG_LOGGER_DEBUG(vast::log(), vast::detail::spd_msg_from_args(__VA_ARGS__).str(), __VA_ARGS__)

#  if VAST_LOG_LEVEL >= VAST_LOG_LEVEL_ERROR
// #    define VAST_ERROR(...)                                                    \
//       VAST_LOG_COMPONENT(VAST_LOG_LEVEL_ERROR, __VA_ARGS__)
#    define VAST_ERROR_ANON(...) VAST_LOG(VAST_LOG_LEVEL_ERROR, __VA_ARGS__)
#  else
//#    define VAST_ERROR(...) VAST_DISCARD_ARGS(__VA_ARGS__)
#    define VAST_ERROR_ANON(...) VAST_DISCARD_ARGS(__VA_ARGS__)
#  endif

#  if VAST_LOG_LEVEL >= VAST_LOG_LEVEL_WARNING
#    define VAST_WARNING(...)                                                  \
      VAST_LOG_COMPONENT(VAST_LOG_LEVEL_WARNING, __VA_ARGS__)
#    define VAST_WARNING_ANON(...) VAST_LOG(VAST_LOG_LEVEL_WARNING, __VA_ARGS__)
#  else
#    define VAST_WARNING(...) VAST_DISCARD_ARGS(__VA_ARGS__)
#    define VAST_WARNING_ANON(...) VAST_DISCARD_ARGS(__VA_ARGS__)
#  endif

#  if VAST_LOG_LEVEL >= VAST_LOG_LEVEL_INFO
#    define VAST_INFO(...) VAST_LOG_COMPONENT(VAST_LOG_LEVEL_INFO, __VA_ARGS__)
#    define VAST_INFO_ANON(...) VAST_LOG(VAST_LOG_LEVEL_INFO, __VA_ARGS__)
#  else
#    define VAST_INFO(...) VAST_DISCARD_ARGS(__VA_ARGS__)
#    define VAST_INFO_ANON(...) VAST_DISCARD_ARGS(__VA_ARGS__)
#  endif

#  if VAST_LOG_LEVEL >= VAST_LOG_LEVEL_VERBOSE
#    define VAST_VERBOSE(...)                                                  \
      VAST_LOG_COMPONENT(VAST_LOG_LEVEL_VERBOSE, __VA_ARGS__)
#    define VAST_VERBOSE_ANON(...) VAST_LOG(VAST_LOG_LEVEL_VERBOSE, __VA_ARGS__)
#  else
#    define VAST_VERBOSE(...) VAST_DISCARD_ARGS(__VA_ARGS__)
#    define VAST_VERBOSE_ANON(...) VAST_DISCARD_ARGS(__VA_ARGS__)
#  endif

#  if VAST_LOG_LEVEL >= VAST_LOG_LEVEL_DEBUG
#    define VAST_DEBUG(...)                                                    \
      VAST_LOG_COMPONENT(VAST_LOG_LEVEL_DEBUG, __VA_ARGS__)
#    define VAST_DEBUG_ANON(...) VAST_LOG(VAST_LOG_LEVEL_DEBUG, __VA_ARGS__)
#  else
#    define VAST_DEBUG(...) VAST_DISCARD_ARGS(__VA_ARGS__)
#    define VAST_DEBUG_ANON(...) VAST_DISCARD_ARGS(__VA_ARGS__)
#  endif

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

#define VAST_ARG_3(x_name, first, last) vast::backwards::make_arg_wrapper(x_name, first, last)


/// Nicely formats a variable or argument. For example, `VAST_ARG(foo)`
/// generates `foo = ...` in log output, where `...` is the content of the
/// variable. `VAST_ARG("size", xs.size())` generates the output
/// `size = xs.size()`.
#define VAST_ARG(...) VAST_PP_OVERLOAD(VAST_ARG_, __VA_ARGS__)(__VA_ARGS__)
