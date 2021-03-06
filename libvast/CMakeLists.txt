# -- bundled dependencies ------------------------------------------------------

# TODO: VAST bundles robin-map in its aux directory, because we need at least
# 0.6.2, which is not widely available on package managers. We should
# investigate switching to a different map, e.g., from folly or abseil.

add_subdirectory(aux/robin-map)
export(
  EXPORT tsl-robin-mapTargets
  FILE tsl-robin-mapTargets.cmake
  NAMESPACE tsl::)
add_custom_target(
  tsl-robin-map-targets-link
  COMMAND
    ${CMAKE_COMMAND} -E create_symlink
    "${CMAKE_CURRENT_BINARY_DIR}/tsl-robin-mapTargets.cmake"
    "${CMAKE_BINARY_DIR}/tsl-robin-mapTargets.cmake"
  COMMENT "Linking tsl-robin-map targets")
install(
  EXPORT tsl-robin-mapTargets
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/vast"
  NAMESPACE tsl::)
string(APPEND VAST_EXTRA_TARGETS_FILES
       "\ninclude(\"\${CMAKE_CURRENT_LIST_DIR}/tsl-robin-mapTargets.cmake\")")

# -- flattbuffers ---------------------------------------------------------------

# TODO: Split into separate library that is linked against libvast publicly,
# e.g., libvast-fbs, which itself links against flatbuffers publicly.

find_package(Flatbuffers REQUIRED CONFIG)
string(APPEND VAST_FIND_DEPENDENCY_LIST
       "\nfind_package(Flatbuffers REQUIRED CONFIG)")
if (TARGET flatbuffers::flatbuffers)
  set(flatbuffers_target flatbuffers::flatbuffers)
elseif (NOT VAST_ENABLE_STATIC_EXECUTABLE AND TARGET
                                              flatbuffers::flatbuffers_shared)
  set(flatbuffers_target flatbuffers::flatbuffers_shared)
else ()
  message(FATAL_ERROR "No suiteable imported target for Flatbuffers found")
endif ()

file(GLOB flatbuffers_schemas CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/vast/fbs/*.fbs"
     "${CMAKE_CURRENT_SOURCE_DIR}/vast/fbs/legacy/*.fbs")
set(flatbuffers_output_path "${CMAKE_CURRENT_BINARY_DIR}/vast/fbs")

add_custom_target(libvast_flatbuffers)

# Translate paths to desired output paths.
foreach (schema ${flatbuffers_schemas})
  get_filename_component(basename ${schema} NAME_WE)
  # The hardcoded path that flatc generates.
  set(output_file "${flatbuffers_output_path}/${basename}_generated.h")
  # The path that we want.
  set(desired_file "${flatbuffers_output_path}/${basename}.hpp")
  # Hackish way to patch generated flatbuffers schemas to support our naming.
  set(rename_${basename}
      ${CMAKE_CURRENT_BINARY_DIR}/flatbuffers_strip_suffix_${basename}.cmake)
  file(
    WRITE ${rename_${basename}}
    "file(READ \"${desired_file}\" include)\n"
    "string(REGEX REPLACE\n"
    "      \"([^\\n]+)_generated.h\\\"\"\n"
    "      \"\\\\1.hpp\\\"\"\n"
    "      new_include \"\${include}\")\n"
    "file(WRITE \"${desired_file}\" \"\${new_include}\")\n")
  # Compile and rename schema.
  add_custom_command(
    OUTPUT ${desired_file}
    COMMAND flatbuffers::flatc -b --cpp --scoped-enums --gen-name-strings -o
            ${flatbuffers_output_path} ${schema}
    COMMAND ${CMAKE_COMMAND} -E rename ${output_file} ${desired_file}
    COMMAND ${CMAKE_COMMAND} -P ${rename_${basename}}
    DEPENDS ${schema}
    COMMENT "Compiling flatbuffers schema ${schema}")
  add_custom_target(flatbuffers_${basename} DEPENDS ${desired_file})
  add_dependencies(libvast_flatbuffers flatbuffers_${basename})
endforeach ()

# -- arrow ---------------------------------------------------------------------

cmake_dependent_option(VAST_ENABLE_ARROW "Build with Apache Arrow support" ON
                       "NOT ${CMAKE_SYSTEM_NAME} STREQUAL \"FreeBSD\"" OFF)
add_feature_info("VAST_ENABLE_ARROW" VAST_ENABLE_ARROW
                 "build with Apache Arrow support.")
if (VAST_ENABLE_ARROW)
  if (NOT ARROW_ROOT_DIR AND VAST_PREFIX)
    set(ARROW_ROOT_DIR ${VAST_PREFIX})
  endif ()
  if (ARROW_ROOT_DIR)
    set(Arrow_ROOT ${ARROW_ROOT_DIR})
  endif ()
  find_package(Arrow 0.17 REQUIRED CONFIG)
  string(APPEND VAST_FIND_DEPENDENCY_LIST
         "\nfind_package(Arrow REQUIRED CONFIG)")
  if (BUILD_SHARED_LIBS)
    set(ARROW_LIBRARY arrow_shared)
  else ()
    set(ARROW_LIBRARY arrow_static)
  endif ()
endif ()

# -- pcap ----------------------------------------------------------------------

option(VAST_ENABLE_PCAP "Build with PCAP support" ON)
add_feature_info("VAST_ENABLE_PCAP" VAST_ENABLE_PCAP "build with PCAP support.")
if (VAST_ENABLE_PCAP)
  if (NOT PCAP_ROOT_DIR AND VAST_PREFIX)
    set(PCAP_ROOT_DIR ${VAST_PREFIX})
  endif ()
  find_package(PCAP REQUIRED)
  if (NOT BUILD_SHARED_LIBS)
    provide_find_module(PCAP)
    string(APPEND VAST_FIND_DEPENDENCY_LIST "\nfind_package(PCAP REQUIRED)")
  endif ()
endif ()

# -- log level -----------------------------------------------------------------

# Choose a deafult log level based on build type.
if (CMAKE_BUILD_TYPE STREQUAL Release)
  set(VAST_LOG_LEVEL_DEFAULT "DEBUG")
elseif (CMAKE_BUILD_TYPE STREQUAL MinSizeRel)
  set(VAST_LOG_LEVEL_DEFAULT "DEBUG")
elseif (CMAKE_BUILD_TYPE STREQUAL RelWithDebInfo)
  set(VAST_LOG_LEVEL_DEFAULT "DEBUG")
else ()
  set(VAST_LOG_LEVEL_DEFAULT "TRACE")
endif ()

# Make sure log level is defined and all-uppercase.
set(VAST_LOG_LEVEL_DOC "maximum log level available at runtime")
if (NOT VAST_LOG_LEVEL)
  set(VAST_LOG_LEVEL
      "${VAST_LOG_LEVEL_DEFAULT}"
      CACHE STRING "${VAST_LOG_LEVEL_DOC}")
elseif (NOT VAST_LOG_LEVEL STREQUAL "$CACHE{VAST_LOG_LEVEL}")
  # Override cached variable when re-running CMake.
  string(TOUPPER "${VAST_LOG_LEVEL}" VAST_LOG_LEVEL)
  set(VAST_LOG_LEVEL
      "${VAST_LOG_LEVEL}"
      CACHE STRING "${VAST_LOG_LEVEL_DOC}" FORCE)
endif ()

set(VAST_CAF_LOG_LEVEL
    "WARNING"
    CACHE STRING ${VAST_LOG_LEVEL_DOC})

# Raise an error for invalid log levels.
set(validLogLevels
    QUIET
    ERROR
    WARNING
    INFO
    VERBOSE
    DEBUG
    TRACE)
list(FIND validLogLevels "${VAST_LOG_LEVEL}" logLevelIndex)
if (logLevelIndex LESS 0)
  message(FATAL_ERROR "Invalid log level: \"${VAST_LOG_LEVEL}\"")
endif ()

# -- caf -----------------------------------------------------------------------

# TODO: Require CAF to be installed.

option(VAST_ENABLE_OPENSSL "Encrypt network communication" ON)
add_feature_info("VAST_ENABLE_OPENSSL" VAST_ENABLE_OPENSSL
                 "encrypt network communication.")

option(VAST_ENABLE_BUNDLED_CAF "Always use the CAF submodule" OFF)
add_feature_info("VAST_ENABLE_BUNDLED_CAF" VAST_ENABLE_BUNDLED_CAF
                 "always use the CAF submodule.")
if (NOT VAST_ENABLE_BUNDLED_CAF)
  # Try to find the required CAF components first...
  find_package(
    CAF 0.17.6 EXACT
    COMPONENTS core io test
    QUIET)
endif ()
if (CAF_FOUND)
  message(STATUS "Found CAF")
  if (VAST_ENABLE_OPENSSL)
    find_package(CAF COMPONENTS openssl)
    if (NOT CAF_LIBRARY_OPENSSL)
      message(
        WARNING
          "Could not locate CAF's OpenSSL module, falling back to bundled CAF")
    else ()
      string(APPEND VAST_FIND_DEPENDENCY_LIST
             "\nfind_package(CAF COMPONENTS openssl REQUIRED)")
    endif ()
  endif ()
  if (CAF_FOUND)
    provide_find_module(CAF)
    string(APPEND VAST_FIND_DEPENDENCY_LIST
           "\nfind_package(CAF COMPONENTS core io test REQUIRED)")
  endif ()
endif ()
if (NOT CAF_FOUND)
  # Use bundled CAF.
  if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/aux/caf/CMakeLists.txt")
    message(
      FATAL_ERROR
        "CAF not found, either use -DCAF_ROOT_DIR=... or initialize the libvast/aux/caf submodule"
    )
  else ()
    set(VAST_ENABLE_BUNDLED_CAF ON)
    set(CAF_LOG_LEVEL ${VAST_CAF_LOG_LEVEL})
    set(CAF_NO_AUTO_LIBCPP TRUE)
    set(CAF_NO_OPENCL TRUE)
    set(CAF_NO_EXAMPLES TRUE)
    set(CAF_NO_UNIT_TESTS TRUE)
    set(CAF_NO_PYTHON TRUE)
    set(CAF_NO_TOOLS TRUE)
    if (NOT VAST_ENABLE_OPENSSL)
      set(CAF_NO_OPENSSL TRUE)
    endif ()
    if (BUILD_SHARED_LIBS)
      set(_linkage_suffix shared)
    else ()
      set(_linkage_suffix static)
      set(CAF_BUILD_STATIC TRUE)
      set(CAF_BUILD_STATIC_ONLY TRUE)
    endif ()
    add_subdirectory(aux/caf)
    set_target_properties(
      libcaf_core_${_linkage_suffix}
      PROPERTIES EXPORT_NAME core
                 CXX_STANDARD 17
                 CXX_STANDARD_REQUIRED ON
                 CXX_EXTENSIONS OFF)
      add_library(CAF::core ALIAS libcaf_core_${_linkage_suffix})
    target_compile_features(libcaf_core_${_linkage_suffix} INTERFACE cxx_std_17)
    set_target_properties(libcaf_io_${_linkage_suffix} PROPERTIES EXPORT_NAME
                                                                  io)
    target_compile_options(
      libcaf_io_${_linkage_suffix} PRIVATE -Wno-maybe-uninitialized
                                           -Wno-unknown-warning-option)
    find_package(Threads REQUIRED)
    target_link_libraries(libcaf_io_${_linkage_suffix} Threads::Threads)
    add_library(CAF::io ALIAS libcaf_io_${_linkage_suffix})
    install(
      TARGETS libcaf_core_${_linkage_suffix} libcaf_io_${_linkage_suffix}
      EXPORT CAFTargets
      ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
      LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    if (VAST_ENABLE_OPENSSL)
      target_include_directories(
        libcaf_openssl_${_linkage_suffix}
        PUBLIC
          $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/aux/caf/libcaf_openssl>
          $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
      set_target_properties(libcaf_openssl_${_linkage_suffix}
                            PROPERTIES EXPORT_NAME openssl)
      add_library(CAF::openssl ALIAS libcaf_openssl_${_linkage_suffix})
      install(
        TARGETS libcaf_openssl_${_linkage_suffix}
        EXPORT CAFTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    endif ()
    add_library(libcaf_test INTERFACE)
    target_link_libraries(libcaf_test INTERFACE CAF::core)
    target_include_directories(
      libcaf_test
      INTERFACE
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/aux/caf/libcaf_test>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
    set_target_properties(libcaf_test PROPERTIES EXPORT_NAME test)
    add_library(CAF::test ALIAS libcaf_test)
    install(
      TARGETS libcaf_test
      EXPORT CAFTargets
      ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
      LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    set(caf_dir ${CMAKE_CURRENT_SOURCE_DIR}/aux/caf)
    export(
      EXPORT CAFTargets
      FILE CAFTargets.cmake
      NAMESPACE CAF::)
    add_custom_target(
      caf-targets-link
      COMMAND
        ${CMAKE_COMMAND} -E create_symlink
        "${CMAKE_CURRENT_BINARY_DIR}/CAFTargets.cmake"
        "${CMAKE_BINARY_DIR}/CAFTargets.cmake"
      COMMENT "Linking CAF targets")
    install(
      EXPORT CAFTargets
      DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/vast"
      NAMESPACE CAF::)
    string(APPEND VAST_EXTRA_TARGETS_FILES
           "\ninclude(\"\${CMAKE_CURRENT_LIST_DIR}/CAFTargets.cmake\")")
    set(CAF_FOUND true)
  endif ()
endif ()

if (VAST_ENABLE_RELOCATABLE_INSTALLATIONS
    AND BUILD_SHARED_LIBS
    AND CAF_LIBRARY_CORE)
  # Copy CAF libraries to installation directory
  get_filename_component(CAF_LIBDIR ${CAF_LIBRARY_CORE} PATH)
  file(GLOB CAF_INSTALLED_LIBRARIES "${CAF_LIBDIR}/libcaf*.so")
  install(FILES ${CAF_INSTALLED_LIBRARIES}
          DESTINATION "${CMAKE_INSTALL_LIBDIR}")
endif ()

# -- source files --------------------------------------------------------------

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/vast/config.hpp.in"
               "${CMAKE_CURRENT_BINARY_DIR}/vast/config.hpp")

file(GLOB_RECURSE libvast_sources CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
     "${CMAKE_CURRENT_SOURCE_DIR}/vast/*.hpp")

# -- libvast -------------------------------------------------------------------

add_library(libvast ${libvast_sources})
add_library(vast::libvast ALIAS libvast)
target_link_libraries(libvast PRIVATE libvast_internal)

set_target_properties(
  libvast
  PROPERTIES INTERPROCEDURAL_OPTIMIZATION_RELEASE ON
             CXX_STANDARD 17
             CXX_STANDARD_REQUIRED ON
             CXX_EXTENSIONS OFF)

set_target_properties(
  libvast
  PROPERTIES SOVERSION "${VAST_VERSION_MAJOR}"
             VERSION "${VAST_VERSION_MAJOR}.${VAST_VERSION_MINOR}"
             OUTPUT_NAME vast)

target_include_directories(
  libvast
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
         $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
         $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

# Link against CAF.
target_link_libraries(libvast PUBLIC CAF::core CAF::io)
if (VAST_ENABLE_OPENSSL)
  target_link_libraries(libvast PUBLIC CAF::openssl)
endif ()
if (VAST_ENABLE_BUNDLED_CAF)
  add_dependencies(libvast caf-targets-link)
endif ()
# Figure out whether we point to a build directory or a prefix.
if (NOT caf_dir)
  set(caf_dir ${CAF_INCLUDE_DIRS})
endif ()
dependency_summary("CAF" "${caf_dir}")

# Link against FlatBuffers.
target_link_libraries(libvast PUBLIC ${flatbuffers_target})
add_dependencies(libvast libvast_flatbuffers)
dependency_summary("FlatBuffers" ${flatbuffers_target})

# Link against yaml-cpp.
find_package(yaml-cpp 0.6.2 REQUIRED HINTS ${YAML_CPP_ROOT})
target_link_libraries(libvast PRIVATE yaml-cpp)
dependency_summary("yaml-cpp" yaml-cpp)

# Link against simdjson.
find_package(simdjson REQUIRED HINTS ${SIMDJSON_ROOT})
target_link_libraries(libvast PUBLIC simdjson::simdjson)
dependency_summary("simdjson" simdjson::simdjson)

# Link against robin-map.
target_link_libraries(libvast PUBLIC tsl::robin_map)
add_dependencies(libvast tsl-robin-map-targets-link)

# Link against Apache Arrow.
if (VAST_ENABLE_ARROW)
  target_link_libraries(libvast PRIVATE ${ARROW_LIBRARY})
  dependency_summary("Apache Arrow" ${ARROW_LIBRARY})
endif ()

# Link against PCAP.
if (VAST_ENABLE_PCAP)
  target_link_libraries(libvast PRIVATE pcap::pcap)
  dependency_summary("PCAP" pcap::pcap)
endif ()

# TODO: Should we move the bundled schemas to libvast?
if (TARGET vast-schema)
  add_dependencies(libvast vast-schema)
endif ()

# Install libvast in PREFIX/lib and headers in PREFIX/include/vast.
install(
  TARGETS libvast
  EXPORT VASTTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(
  DIRECTORY vast
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING
  PATTERN "*.hpp")

# Install generated config and flatbuffers headers.
install(
  DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/vast"
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING
  PATTERN "*.hpp")

add_subdirectory(test)

set(VAST_FIND_DEPENDENCY_LIST
    "${VAST_FIND_DEPENDENCY_LIST}"
    PARENT_SCOPE)
set(VAST_EXTRA_TARGETS_FILES
    "${VAST_EXTRA_TARGETS_FILES}"
    PARENT_SCOPE)
