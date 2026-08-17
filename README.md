# jsbsim_tester

The smallest C++ program that can be shown talking to JSBSim over
FlightGear's UDP wire protocols: it sends a hardcoded control schedule to
JSBSim's UDP `<input>`, receives JSBSim's UDP `<output type="FLIGHTGEAR">`
telemetry, decodes it, and prints it. Nothing else. It exists to demonstrate
*how* a C++ program and JSBSim establish communication, not to be a flight
sim -- every addition (the control schedule, the telemetry print) is kept
from growing large enough to obscure that.

`JSBSim.exe` (from a FlightGear install) runs as its own process, driven
separately by [`run_jsbsim.ps1`](run_jsbsim.ps1); this repo does not embed
or link against JSBSim.

## Requirements

- Windows
- C++17 compiler -- built and tested with MinGW-w64 (GCC) via CMake/Ninja.
  Nothing here is intentionally GCC-specific (CMakeLists.txt has an `MSVC`
  branch too), but only GCC has actually been exercised so far; this is
  meant to be portable to Visual Studio later.
- CMake 3.15+
- JSBSim, from a FlightGear installation (e.g.
  `C:\Program Files\FlightGear <version>\bin\JSBSim.exe`) -- run separately,
  not linked into this program
- No external library dependencies beyond the Windows SDK (winsock2)

## Building

```powershell
cmake -S . -B build -G Ninja
cmake --build build
```

Any CMake generator your toolchain supports should work; Ninja is just what
this was built and tested with. `build\jsbsim_tester.exe` is the result.

If your GCC toolchain doesn't put a plain `ar`/`ranlib` on PATH (some
conda-forge MinGW installs only ship `gcc-ar`/`gcc-ranlib`), CMakeLists.txt
falls back to those automatically -- see the comment there if configure
still fails with `CMAKE_AR-NOTFOUND`.

### Tests

```powershell
ctest --test-dir build --output-on-failure
```

All three test executables (`tests/test_net_fdm.cpp`, `tests/test_control.cpp`,
`tests/test_control_wire.cpp`) run entirely offline: no JSBSim, no network.

## Running

Two processes, two terminals:

```powershell
# Terminal 1: starts JSBSim.exe on scripts/demo.xml
.\run_jsbsim.ps1

# Terminal 2: sends controls, prints telemetry
.\build\jsbsim_tester.exe
```

`run_jsbsim.ps1` looks for `JSBSim.exe` in this order, so no path is
hardcoded anywhere in the repo:

1. `-JsbSim <path>`, if given
2. `$env:JSBSIM_EXE`, if set
3. the newest `C:\Program Files\FlightGear */bin\JSBSim.exe`
4. `JSBSim.exe` on `PATH`

Expect telemetry lines within about a second of both being up, e.g.:

```
t=   5.1  alt=  2833ft  ias=   86kt  pitch=-18.5  roll=-0.0  hdg=0.0  vs=-2856fpm  elev=-0.060  ail=+0.000
```

`jsbsim_tester.exe` prints `NO LINK` instead of a telemetry line whenever no
packet has arrived in the last second -- including, correctly, once
`scripts/demo.xml`'s 120-second run ends and JSBSim exits.

### The control schedule

Hardcoded in [`src/control.cpp`](src/control.cpp) (`controlsAt()`):

| time | elevator | aileron |
|---|---|---|
| t < 5s | 0 | 0 |
| 5s <= t < 15s | -0.06 | 0 |
| 15s <= t < 25s | -0.06 | 0.12 |
| t >= 25s | 0 | 0 |

At t=5s you should see `elev` change and pitch respond; at t=15s, `ail`
change and roll respond. Both settle back down rather than diverge --
[`aircraft/minimal/minimal.xml`](aircraft/minimal/minimal.xml) has no stall
modeling, so the schedule and the aerodynamics were tuned together to stay
inside the range this simplified model behaves believably in. If you ever
change one without the other, rerun a full 120-second live check (see
"Gotchas hit building this" below) before trusting the result.

### 3D view (optional, via fgfs.exe)

```powershell
.\run_jsbsim.ps1 -Fgfs
```

adds a second UDP telemetry stream, port 5510, alongside the one
`jsbsim_tester.exe` reads. In a third terminal:

```powershell
& "C:\Program Files\FlightGear <version>\bin\fgfs.exe" --fdm=external --native-fdm=socket,in,30,,5510,udp --aircraft=c172p
```

`--aircraft` there just picks which 3D model renders the flight; fgfs is
receiving JSBSim's reported position/attitude over the socket, not running
its own FDM, so any aircraft model works.

## How the pieces fit together

```
              controlsAt(t), 20 Hz          FGNetFDM v24, 408 B, big-endian
 main.cpp ------------------------> UDP :5501 ------------------------> JSBSim
     ^                                                                     |
     |                                                                     |
     +----------------------------- UDP :5500 <---------------------------+
              decode() + print, throttled 5 Hz
```

The repo is split into a header-only protocol library and a tester built on
top of it:

- **`include/fgprotocol/`** -- the wire formats, and *only* the wire formats.
  Zero platform dependencies (standard library only: `<array>`, `<cstdint>`,
  `<cmath>`, ...), zero sockets, zero opinions about what to send or what to
  do with what's decoded. This is what makes it genuinely reusable outside
  this repo (see "Using this library elsewhere" below).
  - **`net_fdm.h`**: FlightGear's real native-fdm binary protocol (`FGNetFDM`,
    protocol version 24, exactly 408 bytes, every field big-endian) -- not
    JSBSim's ASCII CSV `<output type="SOCKET">`. `recv()` reads straight into
    a `#pragma pack(1)` `FGNetFDM` struct matching that layout field-for-field,
    and hand-written `ntohf()`/`ntohd()` byte-swap each field out of it
    (deliberately not `<winsock2.h>`'s `ntohl`/`ntohs` -- pulling that header
    in would force every consumer to link against a platform's socket
    library just to decode a struct). A `static_assert(sizeof(FGNetFDM) ==
    408)` is what actually earns trust in the layout: a field-list mistake
    fails the *build*, not a live run. `decode()` is fully header-only
    (`inline`) -- no separate `.cpp` to link. An earlier byte-cursor decoder
    (`BigEndianReader`) took the padding-and-alignment-agnostic approach
    instead, never laying a struct over the wire bytes at all; it's retained
    as test-only reference material in `tests/test_net_fdm.cpp` (not part of
    the library's public surface), cross-checked against `decode()` there so
    it can't silently rot.
  - **`control_wire.h`**: JSBSim's `FGUDPInputSocket` wire format --
    `timestamp,v1,v2,...,vN\n` for an arbitrary ordered list of values, comma
    count must exactly match the declared `<property>` count, and the
    timestamp must strictly increase or the whole packet is dropped silently,
    with no reply. `fgudp_input::buildDatagram()` only knows this format; it
    has no idea what the values mean or how many of them there are supposed
    to be -- that's the caller's job.
- **`src/`** -- the tester: everything about *this* particular test run.
  - **`control.h`/`.cpp`**: which JSBSim properties to drive
    (`kControlProperties`) and what values to send when (`controlsAt()`,
    the hardcoded demo schedule). `buildDatagram()` here is a thin adapter
    packing this demo's 3-value `Controls` struct into the array
    `fgudp_input::buildDatagram()` expects.
  - **`udp_socket.h`/`.cpp`**: non-blocking Winsock sockets, with
    `waitReadable()` wrapping `select()` on a `timeval` so the main loop
    blocks until either a telemetry datagram is queued or a short timeout
    elapses, rather than polling on a fixed sleep. A `WinsockGuard` handles
    `WSAStartup`/`WSACleanup` lifetime. Sockets stay out of the library
    deliberately -- see "Using this library elsewhere" for why.
  - **`main.cpp`**: the loop tying it together -- send controls at 20 Hz,
    drain and decode telemetry, print at 5 Hz.
- **Where the wire config lives**: deliberately *not* in the aircraft file.
  - [`aircraft/minimal/minimal.xml`](aircraft/minimal/minimal.xml) is pure
    flight dynamics -- no `<input>`/`<output>` at all.
  - [`scripts/demo.xml`](scripts/demo.xml) carries the `<input>` (JSBSim has
    no way to declare control input from a standalone directive file).
  - [`output/telemetry.xml`](output/telemetry.xml) and
    [`output/fgfs.xml`](output/fgfs.xml) are standalone *output directive*
    files (root element `<output>`), passed to `JSBSim.exe` via
    `--logdirectivefile`, modelled directly on JSBSim's own
    `data_output/flightgear.xml`. This is what makes the fgfs 3D stream a
    launch-time flag (`-Fgfs`) instead of an aircraft or script edit.

  `src/control.h`'s `kControlProperties` and `scripts/demo.xml`'s `<input>`
  property list have to agree exactly (see above -- silent drop on
  mismatch). Run `jsbsim_tester.exe --print-input-xml` to print the
  `<input>` block generated from the C++ array and diff it against the XML
  if you ever suspect drift.

### Using this library elsewhere

`include/fgprotocol/` doesn't depend on anything else in this repo -- no
sockets, no app-specific structs -- so it can be reused standalone, in
increasing order of effort:

- **Copy-paste vendoring.** Both headers are self-contained (standard
  library only). Copy `net_fdm.h`/`control_wire.h` into another project's
  include tree and `#include` them; no build-system changes needed here at
  all.
- **Git submodule + scoped `add_subdirectory()`.** `include/fgprotocol/` has
  its own standalone `CMakeLists.txt` (not dependent on this repo's root
  file), specifically so a consumer can point at just that directory and not
  drag in `jsbsim_tester`'s own build/tests:
  ```cmake
  add_subdirectory(external/fgprotocol_test/include/fgprotocol)
  target_link_libraries(myapp PRIVATE fgprotocol)
  ```
- **`FetchContent`**, no submodule bookkeeping needed:
  ```cmake
  include(FetchContent)
  FetchContent_Declare(fgprotocol
    GIT_REPOSITORY https://github.com/rtfcv/fgprotocol_test.git
    GIT_TAG        <commit-or-tag>
    SOURCE_SUBDIR  include/fgprotocol)
  FetchContent_MakeAvailable(fgprotocol)
  target_link_libraries(myapp PRIVATE fgprotocol)
  ```

`include/fgprotocol/CMakeLists.txt` also calls its own `project(fgprotocol
LANGUAGES NONE)`, so it configures standalone (`cmake -S include/fgprotocol
-B build`) without requiring a working C/C++ toolchain -- an INTERFACE
library over two headers has nothing to compile.

### Why a hand-written aircraft, not FlightGear's stock c172p

`C:\Program Files\FlightGear <version>\` ships `JSBSim.exe` but no JSBSim
data root; the only JSBSim aircraft in a full FlightGear data install is
`c172p`, and standalone JSBSim can't load it as shipped. Its
`<system file="hydrodynamics"/>` only resolves under FlightGear's own path
search (it lives in `Aircraft/Generic/JSBSim/Systems/`, one level up from
where JSBSim looks), and clearing that just exposes the next problem: the
other stock systems (sounds, bushkit floats, damage, mooring, ...) read
properties FlightGear's Nasal/`-set.xml` layer normally creates first, and a
property read inside JSBSim's `<test>`/`<function>` that was never declared
anywhere is fatal, not a warning. Vendoring and patching all of that (as a
prior sibling project, `fginst`, did) is thousands of lines of aircraft XML
in a repo whose whole point is staying minimal -- so `aircraft/minimal/`
is written from scratch instead: metrics, mass balance, a token ground
contact, simple lift/drag/moment aerodynamics, three control surfaces, no
propulsion. `JSBSim.exe` and (optionally) `fgfs.exe` still come from the
FlightGear install either way.

## Gotchas hit building this

Found by testing against the real `JSBSim.exe` in this FlightGear install,
not by guessing:

- **XML comments can't contain two hyphens in a row anywhere but the closing
  `-->`.** An early draft used a doubled hyphen as a prose separator
  throughout every XML file's comments, which crashed JSBSim before it even
  printed "Reading Aircraft Configuration File" -- just an unhelpful
  "unknown exception". Every comment in this repo's XML avoids that pair
  entirely now.
- **A script-level `<input>` needs a `rate` attribute.** Without one, JSBSim
  crashes silently (no error message at all) partway through printing its
  declared-properties list at startup. `scripts/demo.xml`'s `<input>` sets
  `rate="30"`.
- **A held control input with no compensating static stability can spiral a
  simplified aircraft into numerical blowup.** The first version of
  `aircraft/minimal/minimal.xml` had roll/yaw rate damping but no dihedral
  (`Clbeta`) or weathervane (`Cnbeta`) stability at all; a live end-to-end
  run showed roll rate growing without bound and the simulation diverging to
  nonsense values (airspeed in the billions of knots) around 44 seconds in.
  Fixed by adding both derivatives and increasing the pitch/roll/yaw
  damping terms -- see the comments in that file's ROLL/YAW axes.
- **`JSBSim.exe`'s exit code on a fatal load error is 127, and it can crash
  with zero diagnostic output.** If `run_jsbsim.ps1` or a direct `JSBSim.exe`
  invocation exits without printing much, bisect the aircraft/script XML by
  trimming sections rather than trusting the error message to point at the
  real cause.
- **`git bash`'s `timeout` does not reliably kill native Windows `.exe`
  children.** Discovered while testing this program itself: a `timeout N
  ./jsbsim_tester.exe` that appeared to finish (correct exit code, expected
  line count) can leave the actual process running and bound to its UDP
  ports in the background, silently stealing packets from the *next* test
  run. If a rerun behaves like nothing is arriving over UDP, check
  `Get-Process jsbsim_tester, JSBSim` for stragglers before assuming a code
  bug.
- **A byte-swap helper that takes the wrong parameter type compiles clean and
  corrupts every value.** While switching telemetry decoding to `recv()`
  into a packed `FGNetFDM` struct, `ntohf()`/`ntohd()` were first written to
  take `uint32_t`/`uint64_t`, matching `htonl`-family convention -- but
  called as `ntohf(raw.agl)` where `raw.agl` is a `float`. That's an
  *implicit numeric conversion* (the float's already-nonsense bit-reinterpreted
  value gets rounded/truncated/saturated to an integer), not the intended
  bit-reinterpretation, and neither `/W4` nor `-Wall -Wextra -Wpedantic`
  flagged it. Every decoded field came back wrong -- `longitude_rad` as
  exactly `0` instead of the test's `1.111` -- caught immediately by
  `tests/test_net_fdm.cpp`'s happy-path `CHECK_NEAR` values, not by the
  compiler. Fixed by making `ntohf`/`ntohd` take `float`/`double` directly
  and `memcpy` the bits out internally (`include/fgprotocol/net_fdm.h`).
  This is the exact case CLAUDE.md's "write tests before implementing" rule
  exists for: the build looked fine start to finish.
- **A `recv()` sized to exactly the expected payload silently hides an
  oversize datagram, and hides it differently per platform.** POSIX `recv()`
  truncates a too-large UDP datagram to the buffer size with no error;
  Windows returns `-1`/`WSAEMSGSIZE` instead. Sizing the recv buffer one byte
  past `FGNetFDM` (`RecvBuffer` in `src/main.cpp`, statically asserted to
  `kPacketSize + 1`) makes both cases converge on the same outcome: a
  409+-byte read that `net_fdm::decode()`'s size check rejects as
  `WrongSize`, instead of a truncated 408 bytes of a bigger packet being
  silently decoded as if it were valid.
- **A cross-check test is only as good as its ability to actually fail.**
  [`tests/test_net_fdm.cpp`](tests/test_net_fdm.cpp) decodes every test
  packet two ways -- the live `decode()` (packed struct + `ntoh*`) and
  `decodeWithBigEndianReader()` (the retained byte-cursor decoder, kept
  around specifically as a second, independently-structured implementation
  to check the first against) -- and asserts every field matches. That's
  only meaningful if the test can actually distinguish the two, so this was
  verified directly: deliberately corrupted `BigEndianReader::u32()` (added
  1 to every decoded value), confirmed `ctest` failed on the mismatch, then
  reverted. Worth remembering as a pattern generally: when a rewrite makes a
  prior implementation redundant on the live path, retaining it as a test
  oracle is often better than deleting it -- but only if something actually
  exercises it, and confirming that takes deliberately breaking one side and
  watching the test catch it, not just reading the assertions.

## Future addition (not yet built)

Streaming UDP to `fgfs.exe` for a live 3D view is supported today via
`-Fgfs` (see above) using a second, separate JSBSim output stream. A
from-scratch UDP *sender* implementation (rather than relying on JSBSim's
own dual-output support) remains a possible follow-up if a use case needs
this program itself to originate the fgfs-facing stream.
