#pragma once

#include <caf/deep_to_string.hpp>
#include <caf/event_based_actor.hpp>
#include <caf/stateful_actor.hpp>
#include <caf/string_view.hpp>

#include <string>
#include <type_traits>

#include <spdlog/fmt/ostr.h>

// everything here has been taken form caf
// to make various types logable`
// a long time goal might be to remove this file

namespace caf {
class local_actor;

}

namespace vast::backwards {

class line_builder {
public:
  line_builder() noexcept = default;

  template <typename OStream>
  friend OStream& operator<<(OStream& os, const line_builder& lb) {
    return os << lb.str_;
  }

  template <class T>
  typename std::enable_if<!std::is_pointer<T>::value, line_builder&>::type
  operator<<(const T& x) {
    if (!str_.empty())
      str_ += " ";
    str_ += caf::deep_to_string(x);
    return *this;
  }

  line_builder& operator<<(const caf::local_actor* self);

  line_builder& operator<<(const std::string& str);

  line_builder& operator<<(caf::string_view str);

  line_builder& operator<<(const char* str);

  line_builder& operator<<(char x);

  const std::string& get() const;

private:
  std::string str_;
};

/// Enables automagical string conversion for `CAF_ARG`.
template <class T>
struct single_arg_wrapper {
  const char* name;
  const T& value;
  single_arg_wrapper(const char* x, const T& y) : name(x), value(y) {
    // nop
  }
};

template <class T>
std::string to_string(const single_arg_wrapper<T>& x) {
  std::string result = x.name;
  result += " = ";
  result += caf::deep_to_string(x.value);
  return result;
}

template <class Iterator>
struct range_arg_wrapper {
  const char* name;
  Iterator first;
  Iterator last;
  range_arg_wrapper(const char* x, Iterator begin, Iterator end)
    : name(x), first(begin), last(end) {
    // nop
  }
};

template <class Iterator>
std::string to_string(const range_arg_wrapper<Iterator>& x) {
  std::string result = x.name;
  result += " = ";
  struct dummy {
    Iterator first;
    Iterator last;
    Iterator begin() const {
      return first;
    }
    Iterator end() const {
      return last;
    }
  };
  dummy y{x.first, x.last};
  result += caf::deep_to_string(y);
  return result;
}

/// Used to implement `CAF_ARG`.
template <class T>
single_arg_wrapper<T> make_arg_wrapper(const char* name, const T& value) {
  return {name, value};
}

/// Used to implement `CAF_ARG`.
template <class Iterator>
range_arg_wrapper<Iterator>
make_arg_wrapper(const char* name, Iterator first, Iterator last) {
  return {name, first, last};
}

} // namespace vast::backwards
