#include <caf/message_builder.hpp>
#include <caf/io/middleman.hpp>

#include "vast/batch.hpp"
#include "vast/bitmap.hpp"
#include "vast/error.hpp"
#include "vast/event.hpp"
#include "vast/expression.hpp"
#include "vast/operator.hpp"
#include "vast/query_options.hpp"
#include "vast/schema.hpp"
#include "vast/type.hpp"
#include "vast/uuid.hpp"

#include "vast/system/configuration.hpp"
#include "vast/system/tracker.hpp"

using namespace caf;

namespace vast {
namespace system {

configuration::configuration() {
  // Register VAST's custom types.
  add_message_type<batch>("vast::batch");
  add_message_type<bitmap>("vast::bitmap");
  add_message_type<data>("vast::data");
  add_message_type<event>("vast::event");
  add_message_type<expression>("vast::expression");
  add_message_type<query_options>("vast::query_options");
  add_message_type<relational_operator>("vast::relational_operator");
  add_message_type<schema>("vast::schema");
  add_message_type<type>("vast::type");
  add_message_type<uuid>("vast::uuid");
  // Containers
  add_message_type<std::vector<event>>("std::vector<vast::event>");
  // Actor-specific messages
  add_message_type<component_map>("vast::system::component_map");
  add_message_type<component_map_entry>("vast::system::component_map_entry");
  // Register VAST's custom error type.
  auto renderer = [](uint8_t x, atom_value, const message& msg) {
    std::string result;
    result += "got ";
    switch (static_cast<ec>(x)) {
      default:
        result += to_string(static_cast<ec>(x));
        break;
      case ec::unspecified:
        result += "unspecified error";
        break;
    };
    result += ' ';
    result += deep_to_string(msg);
    return result;
  };
  add_error_category(atom("vast"), renderer);
  // Load modules.
  load<io::middleman>();
}

configuration::configuration(int argc, char** argv) : configuration{} {
  parse(argc, argv);
}

configuration::configuration(const std::vector<std::string>& opts)
  : configuration{} {
  auto opt_msg = message_builder{opts.begin(), opts.end()}.to_message();
  std::istream dummy{nullptr};
  parse(opt_msg, dummy);
}

} // namespace system
} // namespace vast
