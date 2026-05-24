#!/bin/bash

export GEANT4_INSTALL_DIR="/Users/kiara/home/software/built"
export G4SCINTKIT="/Users/kiara/home/research/tambo/G4ScintKit"
export GODDESS="$G4SCINTKIT/goddess-package"
export G4SIPM="$G4SCINTKIT/g4sipm"

# needed by run.sh inside g4scintkit/
export SIMDIR="$G4SCINTKIT/g4scintkit"
export BUILDDIR="$G4SCINTKIT/build/g4scintkit"

# source geant4.sh
_ORIG_DIR="$(pwd)"
cd "${GEANT4_INSTALL_DIR}/bin/"
source "geant4.sh"
cd "$_ORIG_DIR"