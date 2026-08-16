#Requires -Version 5.1
<#
.SYNOPSIS
    Launches JSBSim.exe on scripts/demo.xml, wired up to talk to
    jsbsim_tester.exe (and optionally fgfs.exe) over UDP.

.DESCRIPTION
    No FlightGear/JSBSim path is hardcoded in this repo -- this script
    resolves JSBSim.exe at run time, in order:
      1. -JsbSim <path>, if given
      2. $env:JSBSIM_EXE, if set
      3. the newest "C:\Program Files\FlightGear *\bin\JSBSim.exe"
      4. JSBSim.exe on PATH

.PARAMETER JsbSim
    Explicit path to JSBSim.exe. Overrides every other resolution step.

.PARAMETER Fgfs
    Also pass output/fgfs.xml as a second logdirectivefile, so a separately
    launched fgfs.exe can render the flight in 3D. See README.md for the
    matching fgfs.exe command line.

.EXAMPLE
    .\run_jsbsim.ps1
    Run jsbsim_tester.exe (build\jsbsim_tester.exe) in another terminal.

.EXAMPLE
    .\run_jsbsim.ps1 -Fgfs
    Same, plus a second telemetry stream for a real fgfs.exe.
#>
param(
    [string]$JsbSim,
    [switch]$Fgfs
)

$ErrorActionPreference = "Stop"
$root = $PSScriptRoot

function Resolve-JsbSim {
    param([string]$Explicit)

    if ($Explicit) {
        if (Test-Path $Explicit) { return (Resolve-Path $Explicit).Path }
        throw "-JsbSim path does not exist: $Explicit"
    }

    if ($env:JSBSIM_EXE -and (Test-Path $env:JSBSIM_EXE)) {
        return (Resolve-Path $env:JSBSIM_EXE).Path
    }

    $fgInstalls = Get-ChildItem "C:\Program Files\FlightGear *" -Directory -ErrorAction SilentlyContinue |
        Sort-Object Name -Descending
    foreach ($fg in $fgInstalls) {
        $candidate = Join-Path $fg.FullName "bin\JSBSim.exe"
        if (Test-Path $candidate) { return $candidate }
    }

    $onPath = Get-Command JSBSim.exe -ErrorAction SilentlyContinue
    if ($onPath) { return $onPath.Source }

    throw @"
Could not find JSBSim.exe. Tried, in order:
  1. -JsbSim <path>                                   (not given)
  2. `$env:JSBSIM_EXE                                   (not set, or path missing)
  3. C:\Program Files\FlightGear */bin\JSBSim.exe       (no match)
  4. JSBSim.exe on PATH                                 (not found)

Pass -JsbSim explicitly, e.g.:
  .\run_jsbsim.ps1 -JsbSim "C:\Program Files\FlightGear 2024.1\bin\JSBSim.exe"
"@
}

$exe = Resolve-JsbSim -Explicit $JsbSim
Write-Host "Using JSBSim: $exe"

$scriptPath = Join-Path $root "scripts\demo.xml"
$telemetryDirective = Join-Path $root "output\telemetry.xml"
$fgfsDirective = Join-Path $root "output\fgfs.xml"

$jsbsimArgs = @(
    "--root=$root",
    "--script=$scriptPath",
    "--logdirectivefile=$telemetryDirective",
    "--realtime",
    "--nice"
)
if ($Fgfs) {
    $jsbsimArgs += "--logdirectivefile=$fgfsDirective"
    Write-Host "Also streaming telemetry to port 5510 for fgfs.exe (see README.md)."
}

Write-Host "Run build\jsbsim_tester.exe in another terminal to see it fly."
Write-Host ""

& $exe @jsbsimArgs
exit $LASTEXITCODE
