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

# jsbsim_link: everything that's pure logic and unit-testable without a
# running JSBSim -- the wire decoder and the hardcoded control schedule.
# main.cpp (networking + the console loop) is deliberately NOT in this lib,
# so the test executables below never need Winsock or a socket to build.
add_library(jsbsim_link STATIC
    src/net_fdm.cpp
    src/control.cpp
)
target_include_directories(jsbsim_link PUBLIC ${CMAKE_SOURCE_DIR}/src)

add_executable(jsbsim_tester src/main.cpp src/udp_socket.cpp)
target_link_libraries(jsbsim_tester PRIVATE jsbsim_link)
jsbsim_tester_harden_exe(jsbsim_tester)

if(WIN32)
    target_link_libraries(jsbsim_tester PRIVATE ws2_32)
    target_compile_definitions(jsbsim_tester PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

enable_testing()

add_executable(test_net_fdm tests/test_net_fdm.cpp)
target_link_libraries(test_net_fdm PRIVATE jsbsim_link)
jsbsim_tester_harden_exe(test_net_fdm)
add_test(NAME test_net_fdm COMMAND test_net_fdm)

add_executable(test_control tests/test_control.cpp)
target_link_libraries(test_control PRIVATE jsbsim_link)
jsbsim_tester_harden_exe(test_control)
add_test(NAME test_control COMMAND test_control)
