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

# single_bar/multi_bar read out with the GODDeSS generic photon detector, an
# idealised tile. sipm_model reads the same B3 geometry out with a real g4sipm
# model instead -- the die size then comes from the model rather than from the
# fibre bundle, and the SIPM line gains a `model=` field.
for (spec, file) in ((B3Spec(), "single_bar.manifest"),
                     (B4Spec(), "multi_bar.manifest"),
                     (B3Spec(sipm_model = "hamamatsu-s12573-100c"),
                      "single_bar_g4sipm.manifest"))
    manifest = build_manifest(spec)
    out = write_manifest(joinpath(@__DIR__, file), manifest)
    println("wrote ", basename(out), "  (",
            length(scintillators(manifest)), " scint, ",
            length(fibers(manifest)), " fiber, ",
            length(wraps(manifest)), " wrap, ",
            length(sipms(manifest)), " sipm)")
end
