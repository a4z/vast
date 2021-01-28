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
#include "vast/concept/printable/to_string.hpp"
#include "vast/concept/printable/vast/expression.hpp"
#include "vast/expression_visitors.hpp"

#include "vast/detail/assert.hpp"
#include "vast/detail/overload.hpp"
#include "vast/logger.hpp"

namespace vast {

// -- attribute_extractor ------------------------------------------------------

attribute_extractor::attribute_extractor(caf::atom_value str) : attr{str} {
  // nop
}

bool operator==(const attribute_extractor& x, const attribute_extractor& y) {
  return x.attr == y.attr;
}

bool operator<(const attribute_extractor& x, const attribute_extractor& y) {
  return x.attr < y.attr;
}

// -- field_extractor ------------------------------------------------------------

field_extractor::field_extractor(std::string f) : field{std::move(f)} {
}

bool operator==(const field_extractor& x, const field_extractor& y) {
  return x.field == y.field;
}

bool operator<(const field_extractor& x, const field_extractor& y) {
  return x.field < y.field;
}

// -- type_extractor -----------------------------------------------------------

type_extractor::type_extractor(vast::type t) : type{std::move(t)} {
}

bool operator==(const type_extractor& x, const type_extractor& y) {
  return x.type == y.type;
}

bool operator<(const type_extractor& x, const type_extractor& y) {
  return x.type < y.type;
}

// -- data_extractor -----------------------------------------------------------

data_extractor::data_extractor(vast::type t, vast::offset o)
  : type{std::move(t)}, offset{std::move(o)} {
}

bool operator==(const data_extractor& x, const data_extractor& y) {
  return x.type == y.type && x.offset == y.offset;
}

bool operator<(const data_extractor& x, const data_extractor& y) {
  return std::tie(x.type, x.offset) < std::tie(y.type, y.offset);
}

// -- predicate ----------------------------------------------------------------

predicate::predicate(operand l, relational_operator o, operand r)
  : lhs{std::move(l)}, op{o}, rhs{std::move(r)} {
}

bool operator==(const predicate& x, const predicate& y) {
  return x.lhs == y.lhs && x.op == y.op && x.rhs == y.rhs;
}

bool operator<(const predicate& x, const predicate& y) {
  return std::tie(x.lhs, x.op, x.rhs)
         < std::tie(y.lhs, y.op, y.rhs);
}

// -- curried_predicate --------------------------------------------------------

curried_predicate curried(const predicate& pred) {
  VAST_ASSERT(caf::holds_alternative<data>(pred.rhs));
  return {pred.op, caf::get<data>(pred.rhs)};
}

// -- conjunction --------------------------------------------------------------

conjunction::conjunction(const super& other) : super{other} {
  // nop
}

conjunction::conjunction(super&& other) noexcept : super{std::move(other)} {
  // nop
}

// -- disjunction --------------------------------------------------------------

disjunction::disjunction(const super& other) : super{other} {
  // nop
}

disjunction::disjunction(super&& other) noexcept : super{std::move(other)} {
  // nop
}

// -- negation -----------------------------------------------------------------

negation::negation()
  : expr_{std::make_unique<expression>()} {
}

negation::negation(expression expr)
  : expr_{std::make_unique<expression>(std::move(expr))} {
}

negation::negation(const negation& other)
  : expr_{std::make_unique<expression>(*other.expr_)} {
}

negation::negation(negation&& other) noexcept
  : expr_{std::move(other.expr_)} {
}

negation& negation::operator=(const negation& other) {
  *expr_ = *other.expr_;
  return *this;
}

negation& negation::operator=(negation&& other) noexcept {
  expr_ = std::move(other.expr_);
  return *this;
}

const expression& negation::expr() const {
  return *expr_;
}

expression& negation::expr() {
  return *expr_;
}

bool operator==(const negation& x, const negation& y) {
  return x.expr() == y.expr();
}

bool operator<(const negation& x, const negation& y) {
  return x.expr() < y.expr();
}

// -- expression ---------------------------------------------------------------

const expression::node& expression::get_data() const {
  return node_;
}

expression::node& expression::get_data() {
  return node_;
}

bool operator==(const expression& x, const expression& y) {
  return x.get_data() == y.get_data();
}

bool operator<(const expression& x, const expression& y) {
  return x.get_data() < y.get_data();
}

// -- free functions -----------------------------------------------------------

expression normalize(const expression& expr) {
  expression r;
  r = caf::visit(hoister{}, expr);
  r = caf::visit(aligner{}, r);
  r = caf::visit(denegator{}, r);
  r = caf::visit(deduplicator{}, r);
  r = caf::visit(hoister{}, r);
  return r;
}

caf::expected<expression> normalize_and_validate(const expression& expr) {
  auto normalized = normalize(expr);
  auto result = caf::visit(validator{}, normalized);
  if (!result)
    return result.error();
  return normalized;
}

caf::expected<expression> tailor(const expression& expr, const type& t) {
  if (caf::holds_alternative<caf::none_t>(expr))
    return caf::make_error(ec::unspecified, "invalid expression");
  return caf::visit(type_resolver{t}, expr);
}

namespace {

// Helper function to lookup an expression at a particular offset
const expression* at(const expression* expr, offset::value_type i) {
  VAST_ASSERT(expr != nullptr);
  return caf::visit(detail::overload{
                      [&](const conjunction& xs) -> const expression* {
                        return i < xs.size() ? &xs[i] : nullptr;
                      },
                      [&](const disjunction& xs) -> const expression* {
                        return i < xs.size() ? &xs[i] : nullptr;
                      },
                      [&](const negation& x) -> const expression* {
                        return i == 0 ? &x.expr() : nullptr;
                      },
                      [&](const auto&) -> const expression* { return nullptr; },
                    },
                    *expr);
}

} // namespace <anonymous>

const expression* at(const expression& expr, const offset& o) {
  if (o.empty())
    return nullptr; // empty offsets are invalid
  if (o.size() == 1)
    return o[0] == 0 ? &expr : nullptr; // the root has always offset [0]
  auto ptr = &expr;
  for (size_t i = 1; i < o.size(); ++i) {
    ptr = at(ptr, o[i]);
    if (ptr == nullptr)
      break;
  }
  return ptr;
}

namespace {

bool resolve_impl(std::vector<std::pair<offset, predicate>>& result,
                  const expression& expr, const type& t, offset& o) {
  auto v = detail::overload{
    [&](const auto& xs) { // conjunction or disjunction
      o.emplace_back(0);
      if (!xs.empty()) {
        if (!resolve_impl(result, xs[0], t, o))
          return false;
        for (size_t i = 1; i < xs.size(); ++i) {
          o.back() += 1;
          if (!resolve_impl(result, xs[i], t, o))
            return false;
        }
      }
      o.pop_back();
      return true;
    },
    [&](const negation& x) {
      o.emplace_back(0);
      if (!resolve_impl(result, x.expr(), t, o))
        return false;
      o.pop_back();
      return true;
    },
    [&](const predicate& x) {
      auto resolved = type_resolver{t}(x);
      // Abort on first type error and return a
      // default-constructed vector.
      if (!resolved)
        return false;
      for (auto& pred : caf::visit(predicatizer{}, *resolved))
        result.emplace_back(o, std::move(pred));
      return true;
    },
    [&](caf::none_t) {
      VAST_ASSERT(!"invalid expression node");
      return false;
    },
  };
  return caf::visit(v, expr);
}

} // namespace <anonymous>

std::vector<std::pair<offset, predicate>> resolve(const expression& expr,
                                                  const type& t) {
  std::vector<std::pair<offset, predicate>> result;
  offset o{0};
  if (resolve_impl(result, expr, t, o))
    return result;
  return {};
}

} // namespace vast
