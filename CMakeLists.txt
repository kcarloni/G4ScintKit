cmake_minimum_required(VERSION 3.5 FATAL_ERROR)

# Top-level driver for G4ScintKit: builds GODDESS (vendored as a submodule)
# and the generic g4scint Geant4 application as one project. G4SiPM is
# built separately by its own setup script and located via the G4SIPM_DIR
# variable (consumed inside g4scintkit/CMakeLists.txt).

project(G4ScintKit)

option(BUILD_SHARED_LIBS "Build GODDESS sub-libraries as shared libraries." OFF)

# GODDESS C++ library (defines targets G4PropertyFileReadInTools,
# G4Scintillator, G4Fibre, G4GeneralParticleSource, G4PhotonDetector,
# DataProcessing). Each sub-library exposes its own include directory
# via target_include_directories(... PUBLIC ...), so consumers that
# link against them get the headers automatically.
add_subdirectory(goddess-package/source)

# Generic Geant4 application (defines the g4scint executable; finds
# Geant4 / Boost / zlib / HDF5 / optional G4SiPM internally).
add_subdirectory(g4scintkit)