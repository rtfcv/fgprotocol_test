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

Both test executables (`tests/test_net_fdm.cpp`, `tests/test_control.cpp`)
run entirely offline: no JSBSim, no network.

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

- **Telemetry** (`src/net_fdm.h`/`.cpp`): FlightGear's real native-fdm binary
  protocol (`FGNetFDM`, protocol version 24, exactly 408 bytes, every field
  big-endian) -- not JSBSim's ASCII CSV `<output type="SOCKET">`. Decoding
  goes through a byte-cursor reader that self-checks the total bytes
  consumed, so a field-list mistake shows up as a caught size mismatch
  instead of silently misreading every offset after it.
- **Control** (`src/control.h`/`.cpp`): built for JSBSim's `FGUDPInputSocket`,
  which is strict -- `timestamp,v1,v2,v3\n`, comma count must exactly match
  the declared `<property>` count, and the timestamp must strictly increase
  or the whole packet is dropped silently, with no reply.
- **Sockets** (`src/udp_socket.h`/`.cpp`): a thin non-blocking Winsock
  wrapper plus a `WinsockGuard` for `WSAStartup`/`WSACleanup` lifetime.
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

## Future addition (not yet built)

Streaming UDP to `fgfs.exe` for a live 3D view is supported today via
`-Fgfs` (see above) using a second, separate JSBSim output stream. A
from-scratch UDP *sender* implementation (rather than relying on JSBSim's
own dual-output support) remains a possible follow-up if a use case needs
this program itself to originate the fgfs-facing stream.
