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

#include "vast/concept/convertible/to.hpp"
#include "vast/config.hpp"
#include "vast/data.hpp"
#include "vast/detail/process.hpp"
#include "vast/detail/stable_set.hpp"
#include "vast/detail/system.hpp"
#include "vast/directory.hpp"
#include "vast/error.hpp"
#include "vast/event_types.hpp"
#include "vast/logger.hpp"
#include "vast/path.hpp"
#include "vast/plugin.hpp"
#include "vast/schema.hpp"
#include "vast/system/application.hpp"
#include "vast/system/default_configuration.hpp"

#include <caf/actor_system.hpp>
#include <caf/atom.hpp>
#include <caf/io/middleman.hpp>
#include <caf/settings.hpp>
#include <caf/timestamp.hpp>

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#if VAST_ENABLE_OPENSSL
#  include <caf/openssl/manager.hpp>
#endif

using namespace std::string_literals;
using namespace vast;
using namespace vast::system;

namespace vast::detail {

// TODO: find a better location for this function.
stable_set<path> get_plugin_dirs(const caf::actor_system_config& cfg) {
  stable_set<path> result;
#if !VAST_ENABLE_RELOCATABLE_INSTALLATIONS
  result.insert(path{VAST_LIBDIR} / "vast" / "plugins");
#endif
  // FIXME: we technically should not use "lib" relative to the parent, because
  // it may be lib64 or something else. CMAKE_INSTALL_LIBDIR is probably the
  // best choice.
  if (auto binary = objectpath(nullptr))
    result.insert(binary->parent().parent() / "lib" / "vast" / "plugins");
  else
    VAST_ERROR_ANON(__func__, "failed to get program path");
  if (const char* home = std::getenv("HOME"))
    result.insert(path{home} / ".local" / "lib" / "vast" / "plugins");
  if (auto user_dirs
      = caf::get_if<std::vector<std::string>>(&cfg, "vast.plugin-paths"))
    result.insert(user_dirs->begin(), user_dirs->end());
  return result;
}

} // namespace vast::detail

int main(int argc, char** argv) {
  // Set up our configuration, e.g., load of YAML config file(s).
  default_configuration cfg;
  if (auto err = cfg.parse(argc, argv)) {
    std::cerr << "failed to parse configuration: " << to_string(err)
              << std::endl;
    return EXIT_FAILURE;
  }
  // Application setup.
  auto [root, root_factory] = make_application(argv[0]);
  if (!root)
    return EXIT_FAILURE;
  // Load plugins.
  auto& plugins = plugins::get();
  auto plugin_dirs = detail::get_plugin_dirs(cfg);
  for (const auto& dir : plugin_dirs) {
    VAST_VERBOSE_ANON("looking for plugins in", dir);
    for (const auto& file : directory{dir}) {
      if (file.extension() == ".so" || file.extension() == ".dylib") {
        if (auto plugin = plugin_ptr::make(file.str().c_str())) {
          VAST_ASSERT(*plugin);
          auto has_same_name = [name = (*plugin)->name()](const auto& other) {
            return other->name() == name;
          };
          if (std::none_of(plugins.begin(), plugins.end(), has_same_name)) {
            VAST_VERBOSE_ANON("loaded plugin:", file);
            plugins.push_back(std::move(*plugin));
          } else {
            VAST_ERROR_ANON("failed to load plugin", file,
                            "because another plugin already uses the name",
                            (*plugin)->name());
            return EXIT_FAILURE;
          }
        } else {
          VAST_ERROR_ANON("failed to load plugin:", plugin.error());
        }
      }
    }
  }
  // Initialize successfully loaded plugins.
  for (auto& plugin : plugins) {
    VAST_VERBOSE_ANON("initializing plugin:", plugin->name());
    auto key = "plugins."s + plugin->name();
    if (auto opts = caf::get_if<caf::settings>(&cfg, key)) {
      if (auto config = to<data>(*opts)) {
        VAST_DEBUG_ANON("initializing plugin with options:", *config);
        plugin->initialize(std::move(*config));
      } else {
        VAST_ERROR_ANON("invalid plugin configuration for", plugin->name());
        plugin->initialize(data{});
      }
    } else {
      VAST_DEBUG_ANON("no configuration found for plugin", plugin->name());
      plugin->initialize(data{});
    }
  }
  // Add additional commands from plugins.
  for (auto& plugin : plugins) {
    if (auto cp = plugin.as<command_plugin>()) {
      auto&& [cmd, cmd_factory] = cp->make_command();
      root->add_subcommand(std::move(cmd));
      root_factory.insert(std::make_move_iterator(cmd_factory.begin()),
                          std::make_move_iterator(cmd_factory.end()));
    }
  }
  // Parse CLI.
  auto invocation
    = parse(*root, cfg.command_line.begin(), cfg.command_line.end());
  if (!invocation) {
    if (invocation.error()) {
      render_error(*root, invocation.error(), std::cerr);
      return EXIT_FAILURE;
    }
    // Printing help/documentation returns a no_error, and we want to indicate
    // success when printing the help/documentation texts.
    return EXIT_SUCCESS;
  }
  // Initialize actor system (and thereby CAF's logger).
  if (!init_config(cfg, *invocation, std::cerr))
    return EXIT_FAILURE;
  caf::actor_system sys{cfg};
  fixup_logger(cfg);
  // Print the configuration file(s) that were loaded.
  if (!cfg.config_file_path.empty())
    cfg.config_files.emplace_back(std::move(cfg.config_file_path));
  for (auto& path : cfg.config_files)
    VAST_INFO_ANON("loaded configuration file:", path);
  // Load event types.
  if (auto schema = load_schema(cfg)) {
    event_types::init(*std::move(schema));
  } else {
    VAST_ERROR_ANON("failed to read schema dirs:", render(schema.error()));
    return EXIT_FAILURE;
  }
  // Dispatch to root command.
  auto result = run(*invocation, sys, root_factory);
  if (!result) {
    render_error(*root, result.error(), std::cerr);
    return EXIT_FAILURE;
  }
  if (result->match_elements<caf::error>()) {
    auto& err = result->get_as<caf::error>(0);
    if (err) {
      vast::system::render_error(*root, err, std::cerr);
      return EXIT_FAILURE;
    }
  }
  return EXIT_SUCCESS;
}
