
// have this here for now to see which extra includes are needed

#include "vast/logger_backwards.hpp" // compatible ;-)

#include "vast/path.hpp"
#include "vast/uuid.hpp"
#include "vast/command.hpp"
#include "vast/type.hpp"
#include "vast/view.hpp"


#include "vast/format/multi_layout_reader.hpp"

#define FMT_SAFE_DURATION_CAST 1
#define FMT_USE_INTERNAL 1  // does not work ???
#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>



template <>
struct fmt::formatter<caf::event_based_actor> {

  template <typename ParseContext>
  constexpr auto parse(ParseContext& ctx) {
    return ctx.begin();
  }

  template <typename FormatContext>
  auto format(const caf::event_based_actor& x, FormatContext& ctx) {
    return format_to(ctx.out(), "{}", x.name());
  }
};


template <>
struct fmt::formatter<caf::event_based_actor*> {

  template <typename ParseContext>
  constexpr auto parse(ParseContext& ctx) {
    return ctx.begin();
  }

  template <typename FormatContext>
  auto format(const caf::event_based_actor* x, FormatContext& ctx) {
    return format_to(ctx.out(), "{}", x->name());
  }
};



template <>
struct fmt::formatter<caf::io::broker*> {

  template <typename ParseContext>
  constexpr auto parse(ParseContext& ctx) {
    return ctx.begin();
  }

  template <typename FormatContext>
  auto format(const caf::io::broker* x, FormatContext& ctx) {
    return format_to(ctx.out(), "{}", to_string(x));
  }
};


template <class State, class Base>
struct fmt::formatter<caf::stateful_actor<State,Base>> {

  template <typename ParseContext>
  constexpr auto parse(ParseContext& ctx) {
    return ctx.begin();
  }

  template <typename FormatContext>
  auto format(const caf::stateful_actor<State,Base>   & x, FormatContext& ctx) {
    return format_to(ctx.out(), "{}", x.name());
  }
};



template <class State, class Base>
struct fmt::formatter<caf::stateful_actor<State,Base>*> {

  template <typename ParseContext>
  constexpr auto parse(ParseContext& ctx) {
    return ctx.begin();
  }

  template <typename FormatContext>
  auto format(const caf::stateful_actor<State,Base>*const x, FormatContext& ctx) {
    return format_to(ctx.out(), "{}", x->name());
  }
};



template <>
struct fmt::formatter<vast::path> {

  template <typename ParseContext>
  constexpr auto parse(ParseContext& ctx) {
    return ctx.begin();
  }

  template <typename FormatContext>
  auto format(const vast::path& p, FormatContext& ctx) {
    return format_to(ctx.out(), "{}", p.str());
  }
};


template <>
struct fmt::formatter<vast::uuid> {

  template <typename ParseContext>
  constexpr auto parse(ParseContext& ctx) {
    return ctx.begin();
  }

  template <typename FormatContext>
  auto format(const vast::uuid& p, FormatContext& ctx) {
    return format_to(ctx.out(), "{}", to_string(p));
  }
};


template <>
struct fmt::formatter<vast::record_field> {

  template <typename ParseContext>
  constexpr auto parse(ParseContext& ctx) {
    return ctx.begin();
  }

  template <typename FormatContext>
  auto format(const vast::record_field& p, FormatContext& ctx) {
    return format_to(ctx.out(), "{}", to_string(p));
  }
};

template <>
struct fmt::formatter<vast::predicate> {

  template <typename ParseContext>
  constexpr auto parse(ParseContext& ctx) {
    return ctx.begin();
  }

  template <typename FormatContext>
  auto format(const vast::predicate& p, FormatContext& ctx) {
    return format_to(ctx.out(), "{}", to_string(p));
  }
};

template <>
struct fmt::formatter<vast::bitmap> {

  template <typename ParseContext>
  constexpr auto parse(ParseContext& ctx) {
    return ctx.begin();
  }

  template <typename FormatContext>
  auto format(const vast::bitmap& p, FormatContext& ctx) {
    return format_to(ctx.out(), "{}", to_string(p));
  }
};

template <>
struct fmt::formatter<vast::offset> {

  template <typename ParseContext>
  constexpr auto parse(ParseContext& ctx) {
    return ctx.begin();
  }

  template <typename FormatContext>
  auto format(const vast::offset& p, FormatContext& ctx) {
    return format_to(ctx.out(), "{}", to_string(p));
  }
};





template <>
struct fmt::formatter<vast::format::multi_layout_reader> {

  template <typename ParseContext>
  constexpr auto parse(ParseContext& ctx) {
    return ctx.begin();
  }

  template <typename FormatContext>
  auto format(const vast::format::multi_layout_reader& p, FormatContext& ctx) {
    return format_to(ctx.out(), "{}", to_string(p));
  }
};




template <>
struct fmt::formatter<caf::actor> {

  template <typename ParseContext>
  constexpr auto parse(ParseContext& ctx) {
    return ctx.begin();
  }

  template <typename FormatContext>
  auto format(const caf::actor& p, FormatContext& ctx) {
    return format_to(ctx.out(), "{}", to_string(p));
  }
};


template <>
struct fmt::formatter<vast::invocation> {

  template <typename ParseContext>
  constexpr auto parse(ParseContext& ctx) {
    return ctx.begin();
  }

  template <typename FormatContext>
  auto format(const vast::invocation& p, FormatContext& ctx) {
    return format_to(ctx.out(), "{}", p.name());
  }
};



template <typename T>
struct fmt::formatter<std::vector<T>> {

  template <typename ParseContext>
  constexpr auto parse(ParseContext& ctx) {
    return ctx.begin();
  }

  template <typename FormatContext>
  auto format(const std::vector<T>& p, FormatContext& ctx) {
    return format_to(ctx.out(), "{}", to_string(p));
  }
};

template <>
struct fmt::formatter<vast::record_type> {

  template <typename ParseContext>
  constexpr auto parse(ParseContext& ctx) {
    return ctx.begin();
  }

  template <typename FormatContext>
  auto format(const vast::record_type& val, FormatContext& ctx) {
    vast::backwards::line_builder lb;
    lb << val ;
    return format_to(ctx.out(), "{}", lb.get());
  }
};



// template <>
// struct fmt::formatter<vast::uuid> {

//   template <typename ParseContext>
//   constexpr auto parse(ParseContext& ctx) {
//     return ctx.begin();
//   }

//   template <typename FormatContext>
//   auto format(const vast::uuid& id, FormatContext& ctx) {
//     (void)id;
//     return format_to(ctx.out(), "{}", "TODO");
//   }
// };


template <>
struct fmt::formatter<vast::data_view> {

  template <typename ParseContext>
  constexpr auto parse(ParseContext& ctx) {
    return ctx.begin();
  }

  template <typename FormatContext>
  auto format(const vast::data_view& v, FormatContext& ctx) {
    (void)v;
    return format_to(ctx.out(), "{}", "TODO");
  }
};



template <>
struct fmt::formatter<caf::error> {

  template <typename ParseContext>
  constexpr auto parse(ParseContext& ctx) {
    return ctx.begin();
  }

  template <typename FormatContext>
  auto format(const caf::error& e, FormatContext& ctx) {

    return format_to(ctx.out(), "{}", to_string(e));
  }
};

template <>
struct fmt::formatter<caf::actor_addr> {

  template <typename ParseContext>
  constexpr auto parse(ParseContext& ctx) {
    return ctx.begin();
  }

  template <typename FormatContext>
  auto format(const caf::actor_addr& a, FormatContext& ctx) {

    return format_to(ctx.out(), "{}", to_string(a));
  }
};


template <>
struct fmt::formatter<caf::strong_actor_ptr> {

  template <typename ParseContext>
  constexpr auto parse(ParseContext& ctx) {
    return ctx.begin();
  }

  template <typename FormatContext>
  auto format(const caf::strong_actor_ptr& a, FormatContext& ctx) {

    return format_to(ctx.out(), "{}", to_string(a));
  }
};




template <class T>
struct fmt::formatter<vast::backwards::single_arg_wrapper<T>> {

  template <typename ParseContext>
  constexpr auto parse(ParseContext& ctx) {
    return ctx.begin();
  }

  template <typename FormatContext>
  auto format(const vast::backwards::single_arg_wrapper<T>& val, FormatContext& ctx) {
    return format_to(ctx.out(), "{}", vast::backwards::to_string(val));
  }
};

template <class Iterator>
struct fmt::formatter<vast::backwards::range_arg_wrapper<Iterator>> {

  template <typename ParseContext>
  constexpr auto parse(ParseContext& ctx) {
    return ctx.begin();
  }

  template <typename FormatContext>
  auto format(const vast::backwards::range_arg_wrapper<Iterator>& val, FormatContext& ctx) {
    return format_to(ctx.out(), "{}", vast::backwards::to_string(val));
  }
};

template <class T>
struct fmt::formatter<caf::optional<T>> {

  template <typename ParseContext>
  constexpr auto parse(ParseContext& ctx) {
    return ctx.begin();
  }

  template <typename FormatContext>
  auto format(const caf::optional<T>& val, FormatContext& ctx) {
    return format_to(ctx.out(), "{}", caf::to_string(val));
  }
};





// end up with too many ambigous calls


// template <class T, class Char>
// struct fmt::formatter<
//   T, Char,
//   std::enable_if_t<
//     vast::detail::has_to_string<T>
//      && fmt::detail::type_constant<T, Char>::value == fmt::detail::type::custom_type
//     //  && fmt::internal::type_constant<T, Char>::value == fmt::internal::type::custom_type
//     >> {

//   template <typename ParseContext>
//   constexpr auto parse(ParseContext& ctx) {
//     return ctx.begin();
//   }

//   template <typename FormatContext>
//   auto format(const T& x, FormatContext& ctx) {
//     return format_to(ctx.out(), "{}", to_string(x));
//   }
// };





// it would be so nice if that would work...

// template <typename Whatever>
// struct fmt::formatter<Whatever> {

//   template <typename ParseContext>
//   constexpr auto parse(ParseContext& ctx) {
//     return ctx.begin();
//   }

//   template <typename FormatContext>
//   auto format(const Whatever& val, FormatContext& ctx) {
//     vast::backwards::line_builder lb;
//     lb << val ;
//     return format_to(ctx.out(), "{}", lb.get());
//   }
// };
