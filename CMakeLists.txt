cmake_minimum_required(VERSION 3.15)
project(jsbsim_tester LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/W4)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Some MinGW toolchains (e.g. conda-forge's) put gcc-ar.exe/gcc-ranlib.exe on
# PATH but not the plain ar.exe/ranlib.exe CMake's compiler-id step searches
# for, leaving CMAKE_AR/CMAKE_RANLIB NOTFOUND and any static-library link
# failing with "'CMAKE_AR-NOTFOUND' is not recognized". gcc-ar/gcc-ranlib are
# GCC's own wrappers around the same tools, so falling back to them here is
# equivalent, not a workaround of the actual archiving step.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if(NOT CMAKE_AR OR CMAKE_AR MATCHES "NOTFOUND")
        find_program(JSBSIM_TESTER_GNU_AR NAMES gcc-ar ${CMAKE_CXX_COMPILER_AR})
        if(JSBSIM_TESTER_GNU_AR)
            set(CMAKE_AR "${JSBSIM_TESTER_GNU_AR}" CACHE FILEPATH "Archiver" FORCE)
        endif()
    endif()
    if(NOT CMAKE_RANLIB OR CMAKE_RANLIB MATCHES "NOTFOUND")
        find_program(JSBSIM_TESTER_GNU_RANLIB NAMES gcc-ranlib ${CMAKE_CXX_COMPILER_RANLIB})
        if(JSBSIM_TESTER_GNU_RANLIB)
            set(CMAKE_RANLIB "${JSBSIM_TESTER_GNU_RANLIB}" CACHE FILEPATH "Ranlib" FORCE)
        endif()
    endif()
endif()

# Statically links the MinGW runtime (libgcc/libstdc++/libwinpthread) into
# `target`. Without this, an executable built here only runs from a shell
# whose PATH happens to include the toolchain's bin directory (e.g. the
# conda environment it was built in) -- launched any other way (double
# click, a plain PowerShell/cmd prompt, ctest's own subprocess launch, a
# different shell), Windows' loader can't find libstdc++-6.dll/
# libgcc_s_seh-1.dll (or finds a mismatched one elsewhere on PATH) and the
# process fails immediately: no output, just exit code 0xc0000139
# (STATUS_ENTRYPOINT_NOT_FOUND). Confirmed by hitting exactly that running
# `ctest` before this was applied to the test executables, not just
# jsbsim_tester itself.
function(jsbsim_tester_harden_exe target)
    if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        target_link_options(${target} PRIVATE -static-libgcc -static-libstdc++ -static)
    endif()
endfunction()

# fgprotocol: the header-only FlightGear/JSBSim wire-format library (FGNetFDM
# decode, FGUDPInputSocket encode). Declared in its own standalone
# include/fgprotocol/CMakeLists.txt, not inline here, specifically so
# another project can add_subdirectory()/FetchContent just that directory
# (e.g. via a git submodule) without also configuring jsbsim_tester's own
# executable and tests below. See README.md's "using this library elsewhere"
# section.
add_subdirectory(include/fgprotocol)

# tester_control: this tester's own control policy -- which JSBSim
# properties to drive and what values to send when (src/control.cpp) --
# built on top of fgprotocol's generic datagram encoder. Pure logic,
# unit-testable without a running JSBSim. main.cpp/udp_socket.cpp
# (networking + the console loop) are deliberately NOT in this lib, so the
# test executables below never need Winsock or a socket to build.
add_library(tester_control STATIC src/control.cpp)
target_include_directories(tester_control PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tester_control PUBLIC fgprotocol)

add_executable(jsbsim_tester src/main.cpp src/udp_socket.cpp)
target_link_libraries(jsbsim_tester PRIVATE tester_control)
jsbsim_tester_harden_exe(jsbsim_tester)

if(WIN32)
    target_link_libraries(jsbsim_tester PRIVATE ws2_32)
    target_compile_definitions(jsbsim_tester PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

enable_testing()

# test_net_fdm: net_fdm.h's decode() is fully header-only now, so this only
# needs the library itself, not tester_control.
add_executable(test_net_fdm tests/test_net_fdm.cpp)
target_link_libraries(test_net_fdm PRIVATE fgprotocol)
jsbsim_tester_harden_exe(test_net_fdm)
add_test(NAME test_net_fdm COMMAND test_net_fdm)

# test_control: exercises control.cpp's controlsAt()/buildDatagram() --
# this tester's own policy, through its own public API -- so it still needs
# tester_control.
add_executable(test_control tests/test_control.cpp)
target_link_libraries(test_control PRIVATE tester_control)
jsbsim_tester_harden_exe(test_control)
add_test(NAME test_control COMMAND test_control)

# test_control_wire: exercises fgudp_input::buildDatagram() directly against
# a plain double[], independent of control.cpp's Controls struct -- proves
# the library function itself is decoupled from any specific control scheme.
add_executable(test_control_wire tests/test_control_wire.cpp)
target_link_libraries(test_control_wire PRIVATE fgprotocol)
jsbsim_tester_harden_exe(test_control_wire)
add_test(NAME test_control_wire COMMAND test_control_wire)
