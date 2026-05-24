#!/bin/bash

PATHS="$(dirname "${BASH_SOURCE[0]}")/setup_paths.sh"
[[ -f "$PATHS" ]] || PATHS="${PATHS}.example"
source "$PATHS"

cd "$G4SCINTKIT/build"
make g4scint