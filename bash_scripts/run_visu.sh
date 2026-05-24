#!/bin/bash

source "$(dirname "${BASH_SOURCE[0]}")/setup_paths.sh"

# ==========================================

export OUTDIR="${G4SCINTKIT}/output/visu"
rm -rf $OUTDIR

source "$SIMDIR/run.sh" \
    --outdir $OUTDIR \
    --nevents 0 \
    --trackphotons true \
    "$@"

exit 0