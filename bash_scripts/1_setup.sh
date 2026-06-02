#!/bin/bash

# Source the user's local setup_paths.sh if present, otherwise the
# committed .example (which derives all paths from this script's location
# and reads GEANT4_INSTALL_DIR from the environment).
PATHS="$(dirname "${BASH_SOURCE[0]}")/setup_paths.sh"
[[ -f "$PATHS" ]] || PATHS="${PATHS}.example"
source "$PATHS"

# Build g4sipm first if not already built. The shared-library extension is
# platform dependent (.dylib on macOS, .so on Linux).
case "$(uname -s)" in
    Darwin) _libext=dylib ;;
    *)      _libext=so ;;
esac
if [ ! -f "$G4SIPM/build/g4sipm/libg4sipm.$_libext" ]; then
    "$G4SIPM/1_setup.sh"
fi
unset _libext

mkdir -p "$G4SCINTKIT/build" && cd "$G4SCINTKIT/build"
if [ -f CMakeCache.txt ]; then rm CMakeCache.txt; fi

# Geant4 / Boost are discovered by find_package via standard CMake search.
# Hint with -DGeant4_DIR=... or -DBOOST_ROOT=... here if your install lives
# somewhere CMake won't find on its own.
cmake \
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    -DG4SIPM_DIR="$G4SIPM" \
    "$G4SCINTKIT"
