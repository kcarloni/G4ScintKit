#!/bin/bash

PATHS="$(dirname "${BASH_SOURCE[0]}")/setup_paths.sh"
[[ -f "$PATHS" ]] || PATHS="${PATHS}.example"
source "$PATHS"

# ==========================================

OUTDIR="${G4SCINTKIT}/output/test"
rm -rf "$OUTDIR"

# Geometry is mandatory (run.sh refuses without --manifest). Default to the
# committed single-bar example so this smoke test runs on a fresh checkout with
# no Julia involved; pass your own --manifest to override.
MANIFEST_ARGS=()
if [[ " $* " != *" --manifest "* ]]; then
    MANIFEST_ARGS=(--manifest "${G4SCINTKIT}/g4scintkit/examples/single_bar.manifest")
fi

# Use bash (not source) so run.sh runs in its own shell — its strict-mode
# exit code propagates back to us instead of being masked by a stray exit 0.
bash "$SIMDIR/run.sh" \
    --outdir "$OUTDIR" \
    --trackphotons false \
    "${MANIFEST_ARGS[@]}" \
    "$@"