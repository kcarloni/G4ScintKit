# Regenerate the example manifests in this directory from the designs bundled
# with G4ScintKit.jl (examples/designs/B3.jl and B4.jl).
#
# Run it against any environment that has G4ScintKit available, e.g.
#
#   julia --project=G4ScintKit.jl g4scintkit/examples/generate_examples.jl
#
# (instantiate that environment first: julia --project=G4ScintKit.jl -e
# 'using Pkg; Pkg.instantiate()').
#
# The committed .manifest files are this script's output. They carry
# GODDESS-relative material paths, so they are portable across machines and can
# be run directly by the C++ binary on any checkout that has sourced
# bash_scripts/setup_paths.sh.

using AstroParticleUnits
using G4ScintKit

const DESIGNS = normpath(joinpath(@__DIR__, "..", "..", "G4ScintKit.jl", "examples", "designs"))
include(joinpath(DESIGNS, "B3.jl"))
include(joinpath(DESIGNS, "B4.jl"))

for (spec, file) in ((B3Spec(), "single_bar.manifest"),
                     (B4Spec(), "multi_bar.manifest"))
    manifest = build_manifest(spec)
    out = write_manifest(joinpath(@__DIR__, file), manifest)
    println("wrote ", basename(out), "  (",
            length(scintillators(manifest)), " scint, ",
            length(fibers(manifest)), " fiber, ",
            length(wraps(manifest)), " wrap, ",
            length(sipms(manifest)), " sipm)")
end
