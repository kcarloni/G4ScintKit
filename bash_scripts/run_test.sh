#!/bin/bash

PATHS="$(dirname "${BASH_SOURCE[0]}")/setup_paths.sh"
[[ -f "$PATHS" ]] || PATHS="${PATHS}.example"
source "$PATHS"

# ==========================================

OUTDIR="${G4SCINTKIT}/output/test"
rm -rf "$OUTDIR"

# Use bash (not source) so run.sh runs in its own shell — its strict-mode
# exit code propagates back to us instead of being masked by a stray exit 0.
bash "$SIMDIR/run.sh" \
    --outdir "$OUTDIR" \
    --trackphotons false \
    "$@"