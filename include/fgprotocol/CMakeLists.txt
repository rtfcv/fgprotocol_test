# Standalone on purpose: this file must not reference anything only the
# root CMakeLists.txt defines (no shared cache variables, no assumption
# that CMAKE_SOURCE_DIR is this repo's root). That's what lets another
# project pull in just this library -- via a git submodule pointed at this
# directory with add_subdirectory(), or FetchContent(SOURCE_SUBDIR
# include/fgprotocol) -- without also configuring jsbsim_tester's own
# executable and tests. See README.md's "using this library elsewhere"
# section for the actual usage examples.
cmake_minimum_required(VERSION 3.15)

# LANGUAGES NONE: an INTERFACE library over two headers needs no compiler
# to configure. Requiring CXX here would make `cmake -S include/fgprotocol
# -B build` (a real check that this file works when *this* is the
# top-level project -- the git-submodule/FetchContent reuse case) fail on
# any machine without a working C++ toolchain wired up, for a target that
# never actually compiles anything itself. When add_subdirectory()'d from
# a parent project, that parent's own project() call (with whatever
# languages it needs) already governs the actual build.
project(fgprotocol LANGUAGES NONE)

add_library(fgprotocol INTERFACE)

# ${CMAKE_CURRENT_SOURCE_DIR} is this directory (include/fgprotocol); one
# level up is the include/ that makes #include "fgprotocol/net_fdm.h" and
# #include "fgprotocol/control_wire.h" resolve for anyone linking against
# this target.
target_include_directories(fgprotocol INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/..)

# Not using target_sources(... FILE_SET HEADERS ...) here: that needs CMake
# 3.23+, which would silently raise this library's real minimum version past
# the 3.15 declared above (and past the root project's own minimum) just for
# IDE header visibility. Not worth the portability cost for two files.
