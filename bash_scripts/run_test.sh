#!/bin/bash

source "$(dirname "${BASH_SOURCE[0]}")/setup_paths.sh"

# ==========================================

OUTDIR="${G4SCINTKIT}/output/test"
rm -rf $OUTDIR

source "$SIMDIR/run.sh" \
    --outdir $OUTDIR \
    --trackphotons false \
    "$@"

exit 0