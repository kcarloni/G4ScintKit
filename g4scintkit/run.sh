#!/bin/bash
#
# run.sh — generic g4scint launcher.
#
# Translates a small set of --flag arguments into a Geant4 init file and a
# GPS macro under $OutputDirectory/Input/, then runs the g4scint binary.
# Geometry is supplied via --manifest (required).
#
# This script is safe to re-run on a partially-completed --outdir: it counts
# the events already present in ControlData/inj.data and reduces --nevents
# accordingly.
#
# Requires the env vars BUILDDIR (location of the g4scint binary) and GODDESS
# (path to the goddess-package checkout) to be set — see bash_scripts/setup_paths.sh.

set -eo pipefail

# Capture original argv before parsing — used by --help, --dryrun, and the
# provenance file.
ALL_ARGS=("$@")

usage() {
    cat <<'EOF'
Usage: run.sh --outdir <dir> --manifest <file> [options...]

Required:
  --outdir <dir>            Output directory (created if missing). Re-running
                            against a partially-populated outdir resumes from
                            the next event.
  --manifest <file>         Geometry manifest file (produced by G4ScintKit.jl
                            or hand-written).

Run settings:
  --nevents <N>             Number of events (default: 1; 0 = interactive/visu).
  --trackphotons true|false Track optical photons (default: false).
  --seed <int|clock>        Random seed; "clock" derives from system time
                            (default: 12345).
  --runid <int>             Run ID counter override.
  --filenamephrase <s>      Phrase inserted into the HDF5 filename.
  --quiet true|false        Suppress per-event console output (default: false).

Injection (General Particle Source):
  --injparticle <name>      e.g. "mu-", "e-", "gamma" (default: "mu-").
  --injenergy <"E unit">    e.g. "3 GeV" (default: "3 GeV"). Use _ in place of
                            space if you can't quote (e.g. 3_GeV).
  --injpos "<x y z unit>"   Plane center (default: "0 200 0 mm").
  --injdir "<x y z>"        Surface normal direction (default: "0 -1 0").
  --injthetamin / --injthetamax / --injphimin / --injphimax  "<angle unit>"
  --injrectw / --injrectlen "<dim unit>"   Injection plane size.

Particle list source (overrides GPS):
  --particlelist <csv>      CSV of arbitrary primaries (see C++ Help()).

SiPM:
  --use_g4sipm true|false   Use g4sipm digitization instead of GODDESS PD.
  --sipmmodel <name|path>   SiPM model alias or full path.

Meta:
  --help, -h                Show this message and exit.
  --dryrun                  Generate Run.init + GPS.mac + run_info.txt, then
                            print them and exit without launching g4scint.
EOF
}

# defaults
NumberOfEvents="1"
NeventsSet=false
TrackOpticalPhotons="false"
UseG4Sipm="false"
SipmModel=""
ParticleListFile=""
ManifestFile=""

FilenamePhrase=""
RunID=""
Seed="12345"
Quiet="false"
DryRun="false"

Inj_Particle="mu-"
Inj_Energy="3 GeV"

Inj_Center="0 200 0 mm"
Inj_Direction="0 -1 0"
Inj_ThetaMin="0 deg"
Inj_ThetaMax="0 deg"
Inj_PhiMin="0 deg"
Inj_PhiMax="0 deg"
Inj_Rectangle_Width="0 mm"
Inj_Rectangle_Length="0 mm"

OutputDirectory=""

# ===============================================

# Parse named arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        -h|--help)      usage; exit 0 ;;
        --dryrun)       DryRun="true"; shift ;;
        # run settings
        --outdir)       OutputDirectory="$2"; shift 2 ;;
        --filenamephrase) FilenamePhrase="$2"; shift 2 ;;
        --runid)        RunID="$2"; shift 2 ;;
        --nevents)      NumberOfEvents="$2"; NeventsSet=true; shift 2 ;;
        --trackphotons) TrackOpticalPhotons="$2"; shift 2 ;;
        # injection
        --injparticle)  Inj_Particle="$2"; shift 2 ;;
        --injenergy)    Inj_Energy="$2"; shift 2 ;;
        --injpos)       Inj_Center="$2"; shift 2 ;;
        --injdir)       Inj_Direction="$2"; shift 2 ;;
        --injthetamin)  Inj_ThetaMin="$2"; shift 2 ;;
        --injthetamax)  Inj_ThetaMax="$2"; shift 2 ;;
        --injphimin)    Inj_PhiMin="$2"; shift 2 ;;
        --injphimax)    Inj_PhiMax="$2"; shift 2 ;;
        --injrectw)     Inj_Rectangle_Width="$2"; shift 2 ;;
        --injrectlen)   Inj_Rectangle_Length="$2"; shift 2 ;;
        # particle source
        --particlelist) ParticleListFile="$2"; shift 2 ;;
        # geometry manifest
        --manifest)     ManifestFile="$2"; shift 2 ;;
        # sipm
        --use_g4sipm)   UseG4Sipm="$2"; shift 2 ;;
        --sipmmodel)    SipmModel="$2"; shift 2 ;;
        --seed)         Seed="$2"; shift 2 ;;
        --quiet)        Quiet="$2"; shift 2 ;;
        #
        *)              echo "Unknown option: $1" >&2; usage >&2; exit 1 ;;
    esac
done

Inj_Energy="${Inj_Energy/_/ }"

# Resolve seed: "clock" uses system time, otherwise use the value as-is
if [ "$Seed" = "clock" ]; then
    Seed=$(date +%s)
fi

# For particle list mode, the C++ side handles event/run counts.
# Just ensure we enter batch mode (NumEvents > 0) rather than interactive.
if [ -n "$ParticleListFile" ] && ! $NeventsSet; then
    NumberOfEvents=1
fi

die() { echo "Error: $*" >&2; echo "       (run with --help for usage)" >&2; exit 1; }

# ===== required arguments =====
[ -n "$OutputDirectory" ]                            || die "--outdir is required"
[ -n "$ManifestFile" ]                               || die "--manifest is required"
[ -f "$ManifestFile" ]                               || die "--manifest file not found: $ManifestFile"
[ -z "$ParticleListFile" ] || [ -f "$ParticleListFile" ] \
                                                     || die "--particlelist file not found: $ParticleListFile"

# ===== required env vars =====
: "${BUILDDIR:?BUILDDIR not set — did you source bash_scripts/setup_paths.sh?}"
: "${GODDESS:?GODDESS not set — did you source bash_scripts/setup_paths.sh?}"
[ -x "$BUILDDIR/g4scint" ] || die "g4scint binary not found or not executable: $BUILDDIR/g4scint (did you run bash_scripts/2_compile.sh?)"

# SiPM model alias resolution
resolve_sipmmodel() {
    case "$1" in
        generic)                    echo "generic" ;;
        hamamatsu-s10362-11-100c)   echo "hamamatsu-s10362-11-100c" ;;
        hamamatsu-s10362-33-100c)   echo "hamamatsu-s10362-33-100c" ;;
        hamamatsu-s10362-33-050c)   echo "hamamatsu-s10362-33-050c" ;;
        hamamatsu-s12651-050)       echo "hamamatsu-s12651-050" ;;
        hamamatsu-s12573-100c)      echo "hamamatsu-s12573-100c" ;;
        hamamatsu-s12573-100x)      echo "hamamatsu-s12573-100x" ;;
        *)                          echo "$1" ;;   # pass-through for full file paths
    esac
}

GPS_Filename="inj.data"

# ===============================================

mkdir -p "$OutputDirectory"

# Resumability: if a previous run already produced events, count them and
# reduce NumberOfEvents so we top up to the requested total instead of
# re-running from scratch.
GPS_ControlFile="$OutputDirectory/ControlData/$GPS_Filename"
if [ -f "$GPS_ControlFile" ]; then
    CompletedEvents=$(grep -c '^EventID:' "$GPS_ControlFile" || true)
    if [[ "$CompletedEvents" =~ ^[0-9]+$ ]]; then
        RemainingEvents=$(( NumberOfEvents - CompletedEvents ))
        if [ "$RemainingEvents" -le 0 ]; then
            echo "Detected $CompletedEvents existing events in $GPS_ControlFile,"
            echo "which is >= requested $NumberOfEvents. Nothing to do."
            exit 0
        fi
        echo "Detected $CompletedEvents existing events in $GPS_ControlFile."
        echo "Reducing NumberOfEvents from $NumberOfEvents to $RemainingEvents."
        NumberOfEvents="$RemainingEvents"
    fi
fi

InputDirectory="$OutputDirectory/Input"
rm -rf "$InputDirectory"
mkdir -p "$InputDirectory"

# ------ write GPS macro file -----
GPS_MacFile="$InputDirectory/GPS.mac"
{
    echo "# Macro file for a GeneralParticleSource injection."
    echo "/gps/verbose              0"
    echo "/gps/outFile              $GPS_Filename"
    echo "/gps/particle             $Inj_Particle"
    echo "/gps/energy/eMin          $Inj_Energy"
    echo "/gps/energy/eMax          $Inj_Energy"
    echo "/gps/plane/pos            $Inj_Center"
    echo "/gps/plane/surfaceNormal  $Inj_Direction"
    echo "/gps/plane/shape          rect"
    echo "/gps/plane/posDist        uniform"
    echo "/gps/plane/a              $Inj_Rectangle_Width"
    echo "/gps/plane/b              $Inj_Rectangle_Length"
    echo "/gps/angle/thetaMin       $Inj_ThetaMin"
    echo "/gps/angle/thetaMax       $Inj_ThetaMax"
    echo "/gps/angle/phiMin         $Inj_PhiMin"
    echo "/gps/angle/phiMax         $Inj_PhiMax"
    echo
} > "$GPS_MacFile"

# ------ write .init file -----
InitFile="$InputDirectory/Run.init"
: > "$InitFile"

if [[ "$NumberOfEvents" -gt 0 ]]; then
    echo "--batch $NumberOfEvents" >> "$InitFile"
fi
if [ -n "$ParticleListFile" ]; then
    echo "--useParticleList" >> "$InitFile"
    echo "--particleSourceInput $ParticleListFile" >> "$InitFile"
else
    echo "--particleSourceInput $GPS_MacFile" >> "$InitFile"
fi
echo "--outDir      $OutputDirectory" >> "$InitFile"

if ! $TrackOpticalPhotons; then
    echo "speed up: we won't track optical photons!"
    echo "--noOpticalPhotonTracking" >> "$InitFile"
fi
if [ -n "$FilenamePhrase" ]; then
    echo "--add $FilenamePhrase" >> "$InitFile"
fi
if [ -n "$RunID" ]; then
    echo "--runID $RunID" >> "$InitFile"
fi
echo "--seed $Seed" >> "$InitFile"
echo "will build geometry from manifest: $ManifestFile"
echo "--manifest $ManifestFile" >> "$InitFile"
if $Quiet; then
    echo "--quiet" >> "$InitFile"
fi
if $UseG4Sipm; then
    echo "will use G4SiPM for photon detection"
    echo "--useG4Sipm" >> "$InitFile"
    if [ -n "$SipmModel" ]; then
        Set_SipmModel=$(resolve_sipmmodel "$SipmModel")
        echo "--sipmModel $Set_SipmModel" >> "$InitFile"
    fi
fi

# ------ write provenance file -----
# Records what was run, when, where, and against what code. Written before
# launching g4scint so it survives a crash.
InfoFile="$OutputDirectory/run_info.txt"
{
    echo "# run_info.txt — g4scint provenance"
    echo "timestamp:    $(date -u +%Y-%m-%dT%H:%M:%SZ)"
    echo "host:         $(hostname)"
    echo "user:         ${USER:-unknown}"
    echo "cwd:          $(pwd)"
    echo "argv:         ${ALL_ARGS[*]}"
    echo "BUILDDIR:     $BUILDDIR"
    echo "GODDESS:      $GODDESS"
    echo "binary_mtime: $(stat -f %Sm -t %Y-%m-%dT%H:%M:%S "$BUILDDIR/g4scint" 2>/dev/null \
                         || stat -c %y "$BUILDDIR/g4scint" 2>/dev/null \
                         || echo unknown)"
    if [ -f "$ManifestFile" ]; then
        if command -v shasum >/dev/null 2>&1; then
            echo "manifest_sha: $(shasum -a 256 "$ManifestFile" | awk '{print $1}')"
        elif command -v sha256sum >/dev/null 2>&1; then
            echo "manifest_sha: $(sha256sum "$ManifestFile" | awk '{print $1}')"
        fi
    fi
    # Git provenance: best-effort, never fatal.
    if command -v git >/dev/null 2>&1 && [ -d "${G4SCINTKIT:-}/.git" ]; then
        echo "g4scintkit_sha:  $(git -C "$G4SCINTKIT" rev-parse HEAD 2>/dev/null || echo unknown)"
        echo "g4scintkit_dirty: $(git -C "$G4SCINTKIT" status --porcelain 2>/dev/null | head -c 1 \
                                  | grep -q . && echo yes || echo no)"
        # Submodule SHAs
        git -C "$G4SCINTKIT" submodule status 2>/dev/null \
            | awk '{print "submodule_" $2 ": " $1}'
    fi
} > "$InfoFile"

# ------ dry-run: print artifacts and exit -----
if $DryRun; then
    echo
    echo "=== DRY RUN — not launching g4scint ==="
    echo "--- $InitFile ---"
    cat "$InitFile"
    echo "--- $GPS_MacFile ---"
    cat "$GPS_MacFile"
    echo "--- $InfoFile ---"
    cat "$InfoFile"
    exit 0
fi

# ------ launch g4scint, mirroring stdout/stderr to log.txt -----
LogFile="$OutputDirectory/log.txt"
"$BUILDDIR/g4scint" --init "$InitFile" 2>&1 | tee "$LogFile"
