#ifndef TEST_HPP
#define TEST_HPP

#ifdef SUITE
#define CAF_SUITE SUITE
#endif

#include <set>
#include <string>

#include <caf/test/unit_test.hpp>

// Logging
#define ERROR CAF_TEST_PRINT_ERROR
#define INFO CAF_TEST_PRINT_INFO
#define VERBOSE CAF_TEST_PRINT_VERBOSE
#define MESSAGE CAF_TEST_PRINT_VERBOSE

// Test setup
#define TEST CAF_TEST
#define FIXTURE_SCOPE CAF_TEST_FIXTURE_SCOPE
#define FIXTURE_SCOPE_END CAF_TEST_FIXTURE_SCOPE_END

// Checking
#define REQUIRE CAF_REQUIRE
#define REQUIRE_EQUAL CAF_REQUIRE_EQUAL
#define REQUIRE_NOT_EQUAL CAF_REQUIRE_NOT_EQUAL
#define REQUIRE_LESS CAF_REQUIRE_LESS
#define REQUIRE_LESS_EQUAL CAF_REQUIRE_LESS_EQUAL
#define REQUIRE_GREATER CAF_REQUIRE_GREATER
#define REQUIRE_GREATER_EQUAL CAF_REQUIRE_GREATER_EQUAL
#define CHECK CAF_CHECK
#define CHECK_EQUAL CAF_CHECK_EQUAL
#define CHECK_NOT_EQUAL CAF_CHECK_NOT_EQUAL
#define CHECK_LESS CAF_CHECK_LESS
#define CHECK_LESS_EQUAL CAF_CHECK_LESS_EQUAL
#define CHECK_GREATER CAF_CHECK_GREATER
#define CHECK_GREATER_EQUAL CAF_CHECK_GREATER_EQUAL
#define CHECK_FAIL CAF_CHECK_FAIL
#define FAIL CAF_FAIL

namespace vast {
namespace test {

// Holds global configuration options passed on the command line after the
// special -- delimiter.
extern std::set<std::string> config;

} // namespace test
} // namespace vast

#endif
