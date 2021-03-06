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

#include "vast/expression.hpp"
#define SUITE expression

#include "vast/test/test.hpp"

#include "vast/concept/parseable/to.hpp"
#include "vast/concept/parseable/vast/expression.hpp"
#include "vast/concept/parseable/vast/subnet.hpp"
#include "vast/concept/printable/stream.hpp"
#include "vast/concept/printable/to_string.hpp"
#include "vast/concept/printable/vast/expression.hpp"
#include "vast/fwd.hpp"

#include <caf/sum_type.hpp>

using namespace vast;
using namespace std::string_literals;

TEST(parseable/printable - predicate) {
  predicate pred;
  // LHS: schema, RHS: data
  MESSAGE("x.y.z == 42");
  std::string str = "x.y.z == 42";
  CHECK(parsers::predicate(str, pred));
  CHECK(caf::holds_alternative<field_extractor>(pred.lhs));
  CHECK(caf::holds_alternative<data>(pred.rhs));
  CHECK(caf::get<field_extractor>(pred.lhs) == field_extractor{"x.y.z"});
  CHECK(pred.op == equal);
  CHECK(caf::get<data>(pred.rhs) == data{42u});
  CHECK_EQUAL(to_string(pred), str);
  // LHS: data, RHS: schema
  MESSAGE("T.x == Foo");
  str = "T.x == Foo";
  CHECK(parsers::predicate(str, pred));
  CHECK(caf::holds_alternative<field_extractor>(pred.lhs));
  CHECK(caf::holds_alternative<field_extractor>(pred.rhs));
  CHECK(caf::get<field_extractor>(pred.lhs) == field_extractor{"T.x"});
  CHECK(caf::get<field_extractor>(pred.rhs) == field_extractor{"Foo"});
  CHECK(pred.op == equal);
  CHECK_EQUAL(to_string(pred), str);
  // LHS: data, RHS: data
  MESSAGE("42 in [21, 42, 84]");
  str = "42 in [21, 42, 84]";
  CHECK(parsers::predicate(str, pred));
  CHECK(caf::holds_alternative<data>(pred.lhs));
  CHECK(caf::holds_alternative<data>(pred.rhs));
  CHECK(caf::get<data>(pred.lhs) == data{42u});
  CHECK(pred.op == in);
  CHECK(caf::get<data>(pred.rhs) == data{list{21u, 42u, 84u}});
  CHECK_EQUAL(to_string(pred), str);
  // LHS: type, RHS: data
  MESSAGE("#type != \"foo\"");
  str = "#type != \"foo\"";
  CHECK(parsers::predicate(str, pred));
  CHECK(caf::holds_alternative<attribute_extractor>(pred.lhs));
  CHECK(caf::holds_alternative<data>(pred.rhs));
  CHECK(caf::get<attribute_extractor>(pred.lhs)
        == attribute_extractor{atom::type_v});
  CHECK(pred.op == not_equal);
  CHECK(caf::get<data>(pred.rhs) == data{"foo"});
  CHECK_EQUAL(to_string(pred), str);
  // LHS: data, RHS: type
  MESSAGE("10.0.0.0/8 ni :addr");
  str = "10.0.0.0/8 ni :addr";
  CHECK(parsers::predicate(str, pred));
  CHECK(caf::holds_alternative<data>(pred.lhs));
  CHECK(caf::holds_alternative<type_extractor>(pred.rhs));
  CHECK(caf::get<data>(pred.lhs) == data{*to<subnet>("10.0.0.0/8")});
  CHECK(pred.op == ni);
  CHECK(caf::get<type_extractor>(pred.rhs) == type_extractor{address_type{}});
  CHECK_EQUAL(to_string(pred), str);
  // LHS: type, RHS: data
  MESSAGE(":real >= -4.8");
  str = ":real >= -4.8";
  CHECK(parsers::predicate(str, pred));
  CHECK(caf::holds_alternative<type_extractor>(pred.lhs));
  CHECK(caf::holds_alternative<data>(pred.rhs));
  CHECK(caf::get<type_extractor>(pred.lhs) == type_extractor{real_type{}});
  CHECK(pred.op == greater_equal);
  CHECK(caf::get<data>(pred.rhs) == data{-4.8});
  CHECK_EQUAL(to_string(pred), str);
  // LHS: data, RHS: time
  MESSAGE("now > #timestamp");
  str = "now > #timestamp";
  CHECK(parsers::predicate(str, pred));
  CHECK(caf::holds_alternative<data>(pred.lhs));
  CHECK(caf::holds_alternative<attribute_extractor>(pred.rhs));
  CHECK(pred.op == greater);
  CHECK(caf::get<attribute_extractor>(pred.rhs)
        == attribute_extractor{atom::timestamp_v});
  // LHS: schema, RHS: schema
  MESSAGE("x.a_b == y.c_d");
  str = "x.a_b == y.c_d";
  CHECK(parsers::predicate(str, pred));
  CHECK(caf::holds_alternative<field_extractor>(pred.lhs));
  CHECK(caf::holds_alternative<field_extractor>(pred.rhs));
  CHECK(caf::get<field_extractor>(pred.lhs) == field_extractor{"x.a_b"});
  CHECK(pred.op == equal);
  CHECK(caf::get<field_extractor>(pred.rhs) == field_extractor{"y.c_d"});
  CHECK_EQUAL(to_string(pred), str);
  // Invalid type name.
  MESSAGE(":foo == -42");
  CHECK(!parsers::predicate(":foo == -42"));
}

TEST(parseable - expression) {
  expression expr;
  predicate p1{field_extractor{"x"}, equal, data{42u}};
  predicate p2{type_extractor{real_type{}}, equal, data{real{5.3}}};
  predicate p3{field_extractor{"a"}, greater, field_extractor{"b"}};
  MESSAGE("conjunction");
  CHECK(parsers::expr("x == 42 && :real == 5.3"s, expr));
  CHECK_EQUAL(expr, expression(conjunction{p1, p2}));
  CHECK(parsers::expr("x == 42 && :real == 5.3 && x == 42"s, expr));
  CHECK_EQUAL(expr, expression(conjunction{p1, p2, p1}));
  CHECK(parsers::expr("x == 42 && ! :real == 5.3 && x == 42"s, expr));
  CHECK_EQUAL(expr, expression(conjunction{p1, negation{p2}, p1}));
  CHECK(parsers::expr("x > 0 && x < 42 && a.b == x.y"s, expr));
  CHECK(parsers::expr(
    "#timestamp > 2018-07-04+12:00:00.0 && #timestamp < 2018-07-04+23:55:04.0"s,
    expr));
  auto x = caf::get_if<conjunction>(&expr);
  REQUIRE(x);
  REQUIRE_EQUAL(x->size(), 2u);
  auto x0 = caf::get_if<predicate>(&x->at(0));
  auto x1 = caf::get_if<predicate>(&x->at(1));
  CHECK(caf::holds_alternative<attribute_extractor>(x0->lhs));
  CHECK(caf::holds_alternative<attribute_extractor>(x1->lhs));
  MESSAGE("disjunction");
  CHECK(parsers::expr("x == 42 || :real == 5.3 || x == 42"s, expr));
  CHECK_EQUAL(expr, expression(disjunction{p1, p2, p1}));
  CHECK(parsers::expr("a==b || b==c || c==d"s, expr));
  MESSAGE("negation");
  CHECK(parsers::expr("! x == 42"s, expr));
  CHECK_EQUAL(expr, expression(negation{p1}));
  CHECK(parsers::expr("!(x == 42 || :real == 5.3)"s, expr));
  CHECK_EQUAL(expr, expression(negation{disjunction{p1, p2}}));
  MESSAGE("parentheses");
  CHECK(parsers::expr("(x == 42)"s, expr));
  CHECK_EQUAL(expr, p1);
  CHECK(parsers::expr("((x == 42))"s, expr));
  CHECK_EQUAL(expr, p1);
  CHECK(parsers::expr("x == 42 && (x == 42 || a > b)"s, expr));
  CHECK_EQUAL(expr, expression(conjunction{p1, disjunction{p1, p3}}));
  CHECK(parsers::expr("x == 42 && x == 42 || a > b && x == 42"s, expr));
  expression expected = disjunction{conjunction{p1, p1}, conjunction{p3, p1}};
  CHECK_EQUAL(expr, expected);
  MESSAGE("stray dot regression");
  // This should fail to parse because of the stray dot.
  CHECK(!parsers::expr(
    "#type == \"suricata.http\" && .community_id == \"1:Y3MTSbNCzFAT3I5+i6xzSgrL59k=\""s,
    expr));
}

TEST(parseable - data expression) {
  expression expr;
  CHECK(parsers::expr("42"s, expr));
  auto pred = caf::get_if<predicate>(&expr);
  REQUIRE(pred != nullptr);
  auto extractor = caf::get_if<type_extractor>(&pred->lhs);
  REQUIRE(extractor != nullptr);
  CHECK(caf::holds_alternative<count_type>(extractor->type));
  CHECK(caf::holds_alternative<data>(pred->rhs));
  CHECK_EQUAL(pred->op, equal);
  CHECK(caf::get<data>(pred->rhs) == data{42u});
}
