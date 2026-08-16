## precaution
- this repo is meant to be portable. so do not commit any userspecific path.
- commit as frequent as possible with detailed log, as long as it does not break the working state.
- before implementing write test.
- if unsure, always read the docs

## JSBSim XML rules (violating these crashes JSBSim with little or no diagnostic)
- Never put two hyphens in a row inside an XML comment, except the closing
  `-->`. JSBSim's parser dies on it with an unhelpful "unknown exception"
  and no line number. Don't use `--` as a prose separator in any `.xml`
  file here; use a comma, semicolon, or rephrase instead.
- Any `<input>` element (in an aircraft file or a run script) needs a
  `rate="..."` attribute, or JSBSim dies silently, with no error message,
  partway through printing its declared-properties list at startup.
- After any aircraft or script XML change, actually run `JSBSim.exe`
  against it before trusting it. JSBSim's error messages are frequently
  unhelpful or absent (fatal load errors can exit 127 with no message at
  all). See README.md's "Gotchas hit building this" for the full
  incident writeups, including a flight model that passed loading fine but
  spiral-diverged to numerical garbage only once actually flown.

## Live testing (JSBSim.exe + jsbsim_tester.exe together)
- git bash's `timeout` does not reliably kill native Windows `.exe`
  children. A `timeout N ./jsbsim_tester.exe` that reports a clean exit can
  still leave the process running and bound to its UDP ports in the
  background, silently stealing packets from the next test run. Prefer
  PowerShell `Start-Process -PassThru` / `Stop-Process` for anything you
  need to reliably terminate, and check
  `Get-Process jsbsim_tester, JSBSim -ErrorAction SilentlyContinue` for
  stragglers before concluding a "no telemetry" result means a code bug.
- `scripts/demo.xml`'s run is 120s realtime, then JSBSim exits on its own.
  Launch `jsbsim_tester.exe` immediately after starting JSBSim, not after a
  gap of unrelated tool calls, or the window will have already closed.
