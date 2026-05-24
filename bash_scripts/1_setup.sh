#!/bin/bash

source "$(dirname "$0")/setup_paths.sh"

# Build g4sipm first if not already built
if [ ! -f "$G4SIPM/build/g4sipm/libg4sipm.dylib" ]; then
    "$G4SIPM/1_setup.sh"
fi

mkdir -p "$G4SCINTKIT/build" && cd "$G4SCINTKIT/build"
if [ -f CMakeCache.txt ]; then rm CMakeCache.txt; fi
cmake \
    -DGeant4_DIR="${GEANT4_INSTALL_DIR}/lib/Geant4-10.6.0" \
    -DBOOST_ROOT="/opt/homebrew/Cellar/boost/" \
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    -DG4SIPM_DIR="$G4SIPM" \
    "$G4SCINTKIT"