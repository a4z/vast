#include "vast/program.h"

#include <caf/io/all.hpp>
#include <cstdlib>
#include <csignal>
#include <iostream>
#include "vast/archive.h"
#include "vast/exporter.h"
#include "vast/file_system.h"
#include "vast/id_tracker.h"
#include "vast/index.h"
#include "vast/importer.h"
#include "vast/logger.h"
#include "vast/profiler.h"
#include "vast/receiver.h"
#include "vast/search.h"
#include "vast/signal_monitor.h"
#include "vast/type_info.h"
#include "vast/detail/caf_type_info.h"
#include "vast/detail/type_manager.h"

#ifdef VAST_HAVE_EDITLINE
#include "vast/console.h"
#endif

using namespace caf;

namespace vast {

program::program(configuration config)
  : config_{std::move(config)}
{
}

message_handler program::act()
{
  attach_functor(
      [=](uint32_t)
      {
        receiver_ = invalid_actor;
        tracker_ = invalid_actor;
        archive_ = invalid_actor;
        index_ = invalid_actor;
        search_ = invalid_actor;
      });

  return
  {
    on(atom("run")) >> [=]
    {
      run();
    },
    on(atom("receiver")) >> [=]
    {
      return receiver_;
    },
    on(atom("tracker")) >> [=]
    {
      return tracker_;
    },
    on(atom("archive")) >> [=]
    {
      return archive_;
    },
    on(atom("index")) >> [=]
    {
      return index_;
    },
    on(atom("search")) >> [=]
    {
      return search_;
    },
    on(atom("signal"), arg_match) >> [=](int signal)
    {
      VAST_LOG_ACTOR_VERBOSE("received signal " << signal);
      if (signal == SIGINT || signal == SIGTERM)
        quit(exit::stop);
    }
  };
}

std::string program::describe() const
{
  return "program";
}

void program::run()
{
  auto vast_dir = path{*config_.get("directory")}.complete();

  auto success = logger::instance()->init(
      *logger::parse_level(*config_.get("log.console-verbosity")),
      *logger::parse_level(*config_.get("log.file-verbosity")),
      ! config_.check("log.no-colors"),
      config_.check("log.function-names"),
      vast_dir / "log");

  if (! success)
  {
    std::cerr << "failed to initialize logger" << std::endl;
    quit(exit::error);
    return;
  }

  VAST_LOG_VERBOSE(" _   _____   __________");
  VAST_LOG_VERBOSE("| | / / _ | / __/_  __/");
  VAST_LOG_VERBOSE("| |/ / __ |_\\ \\  / / ");
  VAST_LOG_VERBOSE("|___/_/ |_/___/ /_/  " << VAST_VERSION);
  VAST_LOG_VERBOSE("");

  announce_builtin_types();

  detail::type_manager::instance()->each(
      [&](global_type_info const& gti)
      {
        VAST_LOG_DEBUG("registered type " << gti.id() << ": " << gti.name());
      });

  caf::io::max_msg_size(512 * 1024 * 1024);
  VAST_LOG_ACTOR_DEBUG("set caf maximum message size to " <<
                       caf::io::max_msg_size() / 1024 << " KB");

  try
  {
    if (config_.check("core"))
    {
      *config_["receiver"] = true;
      *config_["tracker"] = true;
      *config_["archive"] = true;
      *config_["index"] = true;
      *config_["search"] = true;
    }

    auto monitor = spawn<signal_monitor, detached+linked>(this);
    send(monitor, atom("act"));

    if (config_.check("profile"))
    {
      auto ms = *config_.as<unsigned>("profile");
      auto prof = spawn<profiler, detached+linked>(
          vast_dir / "log", std::chrono::seconds(ms));

      if (config_.check("profile-cpu"))
        send(prof, atom("start"), atom("perftools"), atom("cpu"));

      if (config_.check("profile-heap"))
        send(prof, atom("start"), atom("perftools"), atom("heap"));

      send(prof, atom("start"), atom("rusage"));
    }

    auto tracker_host = *config_.get("tracker.host");
    auto tracker_port = *config_.as<unsigned>("tracker.port");
    if (config_.check("tracker"))
    {
      tracker_ = spawn<id_tracker_actor>(vast_dir);
      VAST_LOG_ACTOR_INFO(
          "publishes tracker at " << tracker_host << ':' << tracker_port);

      caf::io::publish(tracker_, tracker_port, tracker_host.c_str());
    }
    else if (config_.check("receiver"))
    {
      VAST_LOG_ACTOR_VERBOSE(
          "connects to tracker at " << tracker_host << ':' << tracker_port);
      tracker_ = caf::io::remote_actor(tracker_host, tracker_port);
    }

    auto archive_host = *config_.get("archive.host");
    auto archive_port = *config_.as<unsigned>("archive.port");
    if (config_.check("archive"))
    {
      archive_ = spawn<archive_actor>(
          vast_dir,
          *config_.as<size_t>("archive.max-segments"));

      VAST_LOG_ACTOR_INFO(
          "publishes archive at " << archive_host << ':' << archive_port);

      caf::io::publish(archive_, archive_port, archive_host.c_str());
    }
    else if (config_.check("receiver")
             || config_.check("search")
             || config_.check("index.rebuild"))
    {
      VAST_LOG_ACTOR_VERBOSE(
          "connects to archive at " << archive_host << ':' << archive_port);

      archive_ = caf::io::remote_actor(archive_host, archive_port);
    }

    auto index_host = *config_.get("index.host");
    auto index_port = *config_.as<unsigned>("index.port");
    if (config_.check("index"))
    {
      index_ = spawn<index>(vast_dir, *config_.as<size_t>("index.batch-size"));

      VAST_LOG_ACTOR_INFO(
          "publishes index at " << index_host << ':' << index_port);

      caf::io::publish(index_, index_port, index_host.c_str());
    }
    else if (config_.check("receiver")
             || config_.check("search")
             || config_.check("index.rebuild"))
    {
      VAST_LOG_ACTOR_VERBOSE("connects to index at " <<
                           index_host << ":" << index_port);

      index_ = caf::io::remote_actor(index_host, index_port);
    }

    if (auto partition = config_.get("index.partition"))
    {
      send(index_, atom("partition"), *partition);
    }
    else if (config_.check("index.rebuild"))
    {
      become(
        keep_behavior,
        [=](segment const& s)
        {
          event_id next = s.base() + s.events();
          send(archive_, atom("segment"), next);
          forward_to(index_);
        },
        on(atom("no segment"), arg_match) >> [=](event_id eid)
        {
          VAST_LOG_INFO("sent all segments to index (" << eid - 1 << " events)");
          unbecome();
        });

      VAST_LOG_INFO("begins rebuilding index");
      send(index_, atom("delete"));
      send(archive_, atom("segment"), event_id{1});
    }

    auto search_host = *config_.get("search.host");
    auto search_port = *config_.as<unsigned>("search.port");
    if (config_.check("search"))
    {
      search_ = spawn<search_actor>(vast_dir, archive_, index_);
      VAST_LOG_ACTOR_INFO(
          "publishes search at " << search_host << ':' << search_port);

      caf::io::publish(search_, search_port, search_host.c_str());
    }
    else if (config_.check("receiver")
#ifdef VAST_HAVE_EDITLINE
             || config_.check("console")
#endif
             || config_.check("exporter"))
    {
      VAST_LOG_ACTOR_VERBOSE(
          "connects to search at " << search_host << ":" << search_port);

      search_ = caf::io::remote_actor(search_host, search_port);

#ifdef VAST_HAVE_EDITLINE
      if (config_.check("console"))
      {
        auto c = spawn<console, detached+linked>(search_, vast_dir / "console");
        delayed_send(c, std::chrono::milliseconds(200), atom("prompt"));
      }
#endif
    }

    auto receiver_host = *config_.get("receiver.host");
    auto receiver_port = *config_.as<unsigned>("receiver.port");
    if (config_.check("receiver"))
    {
      receiver_ = spawn<receiver_actor>(tracker_, archive_, index_, search_);
      VAST_LOG_ACTOR_INFO(
          "publishes receiver at " << receiver_host << ':' << receiver_port);

      caf::io::publish(receiver_, receiver_port, receiver_host.c_str());
    }
    else if (config_.check("importer"))
    {
      VAST_LOG_ACTOR_VERBOSE(
          "connects to receiver at " << receiver_host << ":" << receiver_port);

      receiver_ = caf::io::remote_actor(receiver_host, receiver_port);
    }

    actor imp0rter;
    if (config_.check("importer"))
    {
      imp0rter = spawn<importer>(
          vast_dir,
          receiver_,
          *config_.as<size_t>("import.max-events-per-chunk"),
          *config_.as<size_t>("import.max-segment-size") * 1000000,
          *config_.as<size_t>("import.batch-size"));

      if (config_.check("import.submit"))
        send(imp0rter, atom("submit"));
      else
        send(imp0rter, atom("add"),
             *config_.get("import.format"), *config_.get("import.read"));
    }
    else if (config_.check("exporter"))
    {
      auto format = *config_.get("export.format");
      auto out = *config_.get("export.write");
      auto limit = *config_.as<uint64_t>("export.limit");
      auto exp0rter = spawn<exporter, linked>();

      send(exp0rter, atom("add"), format, out);
      if (limit > 0)
        send(exp0rter, atom("limit"), limit);

      auto query = *config_.get("export.query");
      sync_send(search_, atom("query"), exp0rter, query).then(
          on_arg_match >> [=](error const& e)
          {
            VAST_LOG_ACTOR_ERROR("got invalid query syntax: " << e);
            std::cerr  << "syntax error: " << e << std::endl;
            quit(exit::error);
          },
          [=](expr::ast const& ast, actor qry)
          {
            VAST_LOG_ACTOR_DEBUG("instantiated query for: " << ast);
            exp0rter->link_to(qry);
            send(qry, atom("extract"), limit);
          },
          others() >> [=]
          {
            VAST_LOG_ACTOR_ERROR("got unexpected reply: " <<
                                 to_string(last_dequeued()));

            quit(exit::error);
          });
    }

    if (config_.check("receiver"))
    {
      // We always initiate the shutdown via the receiver, regardless of
      // whether we have an importer in our process.
      link_to(receiver_);
      receiver_->link_to(tracker_);
      receiver_->link_to(archive_);
      receiver_->link_to(index_);
      receiver_->link_to(search_);

      // We're running in "one-shot" mode where both importer and receiver
      // share the same program. In this case we initiate the teardown
      // via the importer as this ensures proper delivery of inflight segments
      // from importer to receiver.
      if (imp0rter)
        imp0rter->link_to(receiver_);
    }
    else if (imp0rter && ! config_.check("receiver"))
    {
      // If we're running in ingestion mode, we're independent and terminate as
      // soon as the importer has finished.
      link_to(imp0rter);
    }
  }
  catch (network_error const& e)
  {
    VAST_LOG_ACTOR_ERROR("encountered network error: " << e.what());
    quit(exit::error);
  }
}

} // namespace vast
