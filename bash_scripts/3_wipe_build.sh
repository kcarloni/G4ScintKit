#!/bin/bash
#
# Wipe all generated build directories so the next 1_setup.sh reconfigures
# from scratch. Use this when something in the environment changes underneath
# CMake's cache — most importantly switching Geant4 versions, which otherwise
# leaves a stale g4sipm/build configured against the old Geant4 (the
# 1_setup.sh guard only checks whether libg4sipm exists, not what it was
# built against).
#
# Paths are derived from this script's location; no environment setup needed.

G4SCINTKIT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# Both build trees: the main project (g4scint + in-tree GODDESS) and g4sipm
# (which also holds its externals/ build). GODDESS has no separate build dir —
# it compiles under build/goddess-package/ via add_subdirectory.
for _d in "$G4SCINTKIT/build" "$G4SCINTKIT/g4sipm/build"; do
    if [[ -d "$_d" ]]; then
        echo "wiping  $_d"
        rm -rf "$_d"
    else
        echo "absent  $_d"
    fi
done
