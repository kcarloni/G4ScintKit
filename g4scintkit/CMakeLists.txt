#/*
# * author:      Erik Dietz-Laursonn
# * institution: Physics Institute 3A, RWTH Aachen University, Aachen, Germany
# * copyright:   Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License
# */


########## Setup the project ##########
cmake_minimum_required(VERSION 2.6 FATAL_ERROR)

# Set project name:
project(g4scint)

########## Add external libraries ##########
#------ Geant4 ------#
# Find Geant4 package, activating all available UI and Vis drivers by default
option(WITH_GEANT4_UIVIS "Build with all Geant4 UIs and all visualization drivers" ON)
option(WITH_GEANT4_OPENGL "Build with Geant4 shell UI and OpenGl visualization" OFF)
option(WITH_GEANT4_QT "Build with Geant4 Qt UI" OFF)

if(WITH_GEANT4_OPENGL)
	find_package(Geant4 REQUIRED vis_opengl_x11 ui_tcsh)
elseif(WITH_GEANT4_QT)
	find_package(Geant4 REQUIRED qt)
elseif(WITH_GEANT4_UIVIS)
	find_package(Geant4 REQUIRED ui_all vis_all)
else()
	find_package(Geant4 REQUIRED)
endif()

# Add Geant4 include directories and compile definitions to the list of directories searched by the compiler for include files:
include(${Geant4_USE_FILE})

# Set Geant4 library list:
# set(LIBS_GEANT4 ${Geant4_LIBRARIES})   // NOTE: These libraries are already included by the G4BasicObjects of GODDeSS and an additional inclusion here leads to linking errors since Geant4.10!

#------ Boost ------#

# Find Boost package
# Add include directories to the list of directories searched by the compiler for include files:
# Set Boost library list
if(NOT Boost_FOUND)
    find_package(Boost REQUIRED COMPONENTS regex)
    include_directories(${Boost_INCLUDE_DIRS})
endif()
set(LIBS_BOOST ${Boost_REGEX_LIBRARY})

#------ zlib ------#
# Find zlib package
find_package(ZLIB REQUIRED)

# Add include directories to the list of directories searched by the compiler for include files:
include_directories(${ZLIB_INCLUDE_DIRS})

# Set zlib library list
set(LIBS_ZLIB ${ZLIB_LIBRARIES})

#------ HDF5 ------#
find_package(HDF5 REQUIRED COMPONENTS CXX)
include_directories(${HDF5_INCLUDE_DIRS})
set(LIBS_HDF5 ${HDF5_CXX_LIBRARIES} ${HDF5_LIBRARIES})

#------ G4SiPM ------#
if(DEFINED G4SIPM_DIR)
    find_library(LIBS_G4SIPM NAMES g4sipm HINTS "${G4SIPM_DIR}/build/g4sipm")
    include_directories("${G4SIPM_DIR}/g4sipm/include")
    add_compile_definitions(USE_G4SIPM)
    message(STATUS "Found G4SiPM library: ${LIBS_G4SIPM}")
endif()

########## Add libraries of the current project ##########
# DataProcessing is the top of the goddess-package dependency graph; linking
# it pulls in the other five sub-libraries (G4PropertyFileReadInTools,
# G4Scintillator, G4Fibre, G4GeneralParticleSource, G4PhotonDetector)
# transitively, and propagates each one's PUBLIC include directories.
set(LIBS DataProcessing)

########## The project ##########
# Get list of needed header and source files:
file(GLOB sources ${PROJECT_SOURCE_DIR}/src/DataProcessing/*.cc ${PROJECT_SOURCE_DIR}/src/Preparation/*.cc)
file(GLOB headers ${PROJECT_SOURCE_DIR}/include/DataProcessing/*.hh ${PROJECT_SOURCE_DIR}/include/Preparation/*.hh)

# Add include directories to the list of directories searched by the compiler for include files:
include_directories(BEFORE
	${PROJECT_SOURCE_DIR}/include/DataProcessing 
	${PROJECT_SOURCE_DIR}/include/Preparation
)

# Create the executable and add links and dependencies (inside the current project):
add_executable(${PROJECT_NAME} RunSimulation.cc ${sources} ${headers})
target_link_libraries(${PROJECT_NAME} ${LIBS_GEANT4} ${LIBS_BOOST} ${LIBS_ZLIB} ${LIBS_HDF5} ${LIBS} ${LIBS_G4SIPM})
add_dependencies(${PROJECT_NAME} ${LIBS})

# Install the executable to the destination directory:
set(INSTALL_DIRECTORY ${CMAKE_BINARY_DIR})
install(TARGETS ${PROJECT_NAME} DESTINATION ${INSTALL_DIRECTORY})

# add dependencies to the documentation (defined in the top-level cmake file):
if(TARGET documentation)
    add_dependencies(documentation ${PROJECT_NAME})
endif()
