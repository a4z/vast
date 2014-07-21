#include "vast/configuration.h"
#include "vast/file_system.h"
#include "vast/logger.h"
#include "vast/string.h"
#include "vast/type_info.h"
#include "vast/detail/type_manager.h"
#include "vast/util/color.h"

namespace vast {

std::string configuration::banner() const
{
  std::stringstream ss;
  auto colorize = ! check("log.no-colors");
  if (colorize)
    ss << util::color::red;

  ss << "     _   _____   __________\n"
        "    | | / / _ | / __/_  __/\n"
        "    | |/ / __ |_\\ \\  / /\n"
        "    |___/_/ |_/___/ /_/  ";

  if (colorize)
    ss << util::color::yellow;

  ss << VAST_VERSION;

  if (colorize)
    ss << util::color::reset;

  return ss.str();
}

void configuration::initialize()
{
  auto& general = create_block("general options");
// TODO: not yet supported.
//  general.add('c', "config", "configuration file");
  general.add('h', "help", "display this help");
  general.add('d', "directory", "VAST directory").init("vast");
  general.add('z', "advanced", "show advanced options");
  general.add("version", "print the version of VAST");

  auto min = 0;
  auto max = VAST_LOG_LEVEL;
  auto range = '[' + std::to_string(min) + '-' + std::to_string(max) + ']';

  auto& log = create_block("logger options", "log");
  log.add('v', "console-verbosity", "console verbosity " + range)
     .init(std::min(3, max));
  log.add('V', "file-verbosity", "log file verbosity " + range)
     .init(std::min(4, max));
  log.add("no-colors", "don't use colors for console output");
  log.add("function-names", "log function names");

  auto& advanced = create_block("advanced options");
  advanced.add('P', "profile",
               "spawn the profiler with at a given granularity (seconds)")
          .single();
#ifdef VAST_USE_PERFTOOLS_CPU_PROFILER
  advanced.add("profile-cpu", "also enable Google perftools CPU profiling");
#endif
#ifdef VAST_USE_PERFTOOLS_HEAP_PROFILER
  advanced.add("profile-heap", "also enable Google perftools heap profiling");
#endif
  advanced.visible(false);

  auto& act = create_block("actor options");
  act.add('C', "core", "spawn all core actors");
//  act.add('R', "receiver", "spawn the receiver");
//  act.add('A', "archive", "spawn the archive");
//  act.add('X', "index", "spawn the index");
//  act.add('T', "tracker", "spawn the tracker");
//  act.add('S', "search", "spawn the search");
  act.add('E', "exporter", "spawn the exporter");
  act.add('I', "importer", "spawn the importer");
#ifdef VAST_HAVE_EDITLINE
  act.add('Q', "console", "spawn the query console");
#endif

  auto& imp = create_block("import options", "import");
  imp.add("max-events-per-chunk", "maximum events per chunk").init(5000);
  imp.add("max-segment-size", "maximum segment size in MB").init(128);
  imp.add("batch-size", "number of events to ingest in one run").init(4000);
  imp.add('r', "read", "path to input file/directory").init("-");
  imp.add('i', "format", "format of events to ingest").init("bro");
  imp.add("submit", "send orphaned segments on startup");
  imp.visible(false);

  auto& exp = create_block("export options", "export");
  exp.add('l', "limit", "maximum number of results").init(0);
  exp.add('o', "format", "format of events to generate").init("bro");
  exp.add('q', "query", "the query string").single();
  exp.add('w', "write", "path to output file/directory").init("-");
  exp.visible(false);

  auto& recv = create_block("receiver options", "receiver");
  recv.add("host", "hostname/address of the receiver").init("127.0.0.1");
  recv.add("port", "TCP port of the receiver").init(42000);
  recv.visible(false);

  auto& arch = create_block("archive options", "archive");
  arch.add("max-segments", "maximum segments cached in memory").init(10);
  arch.add("host", "hostname/address of the archive").init("127.0.0.1");
  arch.add("port", "TCP port of the archive").init(42003);
  arch.visible(false);

  auto& idx = create_block("index options", "index");
  idx.add('p', "partition", "name of the partition to append to").single();
  idx.add("batch-size", "number of events to index in one run").init(1000);
  idx.add("rebuild", "rebuild indexes from archive");
  idx.add("host", "hostname/address of the archive").init("127.0.0.1");
  idx.add("port", "TCP port of the index").init(42004);
  idx.visible(false);

  auto& track = create_block("ID tracker options", "tracker");
  track.add("host", "hostname/address of the tracker").init("127.0.0.1");
  track.add("port", "TCP port of the ID tracker").init(42002);
  track.visible(false);

  auto& srch = create_block("search options", "search");
  srch.add("host", "hostname/address of the archive").init("127.0.0.1");
  srch.add("port", "TCP port of the search").init(42001);
  srch.visible(false);

#ifdef VAST_HAVE_EDITLINE
  add_conflict("console", "core");
  add_conflict("console", "tracker");
  add_conflict("console", "archive");
  add_conflict("console", "index");
  add_conflict("console", "importer");
  add_conflict("console", "exporter");
  add_conflict("console", "search");
  add_conflict("console", "receiver");
#endif

  add_dependency("import.submit", "importer");
  add_dependency("import.read", "importer");
  add_dependency("import.format", "import.read");

  add_dependencies("index.partition", {"index", "core"});
  add_conflict("index.rebuild", "index.partition");

  add_conflict("importer", "exporter");
  add_conflict("receiver", "exporter");
  add_conflict("tracker", "exporter");

  add_dependency("export.limit", "exporter");
  add_dependency("export.format", "exporter");
  add_dependency("export.query", "exporter");
  add_dependency("export.write", "exporter");
}

} // namespace vast
