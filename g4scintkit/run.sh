#!/bin/bash

# defaults
SetupID="B1"
NumberOfEvents="1"
NeventsSet=false
TrackOpticalPhotons="false"
UseG4Sipm="false"
SipmModel=""
ParticleListFile=""
ManifestFile=""

FilenamePhrase=""
RunID=""
Seed="12345"
Quiet="false"

Inj_Particle="mu-"
Inj_Energy="3 GeV"

Scint_Thickness_mm="9.5"
Scint_Length_mm="1875.0"
Scint_Width_mm="49.5"

Lead_Thickness_mm="0.0"
Aluminum_Thickness_mm="0.0"

Inj_Center="0 200 0 mm"
Inj_Direction="0 -1 0"
Inj_ThetaMin="0 deg"
Inj_ThetaMax="0 deg"
Inj_PhiMin="0 deg"
Inj_PhiMax="0 deg"
Inj_Rectangle_Width="0 mm"
Inj_Rectangle_Length="0 mm"

# material defaults (abbreviations)
Mat_Scint="fermilab"
Mat_Wrap="tio2"
Mat_Twin="alu"
Mat_WLS="y11-300-r1"
Mat_LG="bcf98-r1"
Mat_Cement="air1mm"

# ===============================================

# Parse named arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        # run settings
        --outdir)       OutputDirectory="$2"; shift 2 ;;
        --filenamephrase)     FilenamePhrase="$2"; shift 2 ;;
        --runid)        RunID="$2"; shift 2 ;;
        --setup)        SetupID="$2"; shift 2 ;;
        --nevents)      NumberOfEvents="$2"; NeventsSet=true; shift 2 ;;
        --trackphotons) TrackOpticalPhotons="$2"; shift 2 ;;
        # injection
        --injparticle)  Inj_Particle="$2"; shift 2 ;;
        --injenergy)    Inj_Energy="$2"; shift 2 ;;
        --injpos)       Inj_Center="$2"; shift 2 ;;
        --injdir)       Inj_Direction="$2"; shift 2 ;;
        --injthetamin)  Inj_ThetaMin="$2"; shift 2 ;;
        --injthetamax)  Inj_ThetaMax="$2"; shift 2 ;;
        --injphimin)    Inj_PhiMin="$2"; shift 2 ;;
        --injphimax)    Inj_PhiMax="$2"; shift 2 ;;
        --injrectw)     Inj_Rectangle_Width="$2"; shift 2 ;;
        --injrectlen)   Inj_Rectangle_Length="$2"; shift 2 ;;
        # geometry
        --leadth)       Lead_Thickness_mm="$2"; shift 2 ;;
        --aluth)        Aluminum_Thickness_mm="$2"; shift 2 ;;
        --scintth)      Scint_Thickness_mm="$2"; shift 2 ;;
        --scintlen)     Scint_Length_mm="$2"; shift 2 ;;
        --scintw)       Scint_Width_mm="$2"; shift 2 ;;
        # materials
        --matscint)     Mat_Scint="$2"; shift 2 ;;
        --matwrap)      Mat_Wrap="$2"; shift 2 ;;
        --mattwin)      Mat_Twin="$2"; shift 2 ;;
        --matwls)       Mat_WLS="$2"; shift 2 ;;
        --matlg)        Mat_LG="$2"; shift 2 ;;
        --matcement)    Mat_Cement="$2"; shift 2 ;;
        # particle source
        --particlelist) ParticleListFile="$2"; shift 2 ;;
        # geometry manifest (overrides --setup)
        --manifest)     ManifestFile="$2"; shift 2 ;;
        # sipm
        --use_g4sipm)   UseG4Sipm="$2"; shift 2 ;;
        --sipmmodel)    SipmModel="$2"; shift 2 ;;
        --seed)         Seed="$2"; shift 2 ;;
        --quiet)        Quiet="$2"; shift 2 ;;
        #
        *)              echo "Unknown option: $1"; exit 1 ;;
    esac
done

Inj_Energy="${Inj_Energy/_/ }"

# Resolve seed: "clock" uses system time, otherwise use the value as-is
if [ "$Seed" = "clock" ]; then
    Seed=$(date +%s)
fi

# For particle list mode, the C++ side handles event/run counts.
# Just ensure we enter batch mode (NumEvents > 0) rather than interactive.
if [ -n "$ParticleListFile" ] && ! $NeventsSet; then
    NumberOfEvents=1
fi

if [ -z "$OutputDirectory" ]; then
    echo "Error: --outdir is required"; exit 1
fi

# ===============================================
# Material abbreviation mappings
# ===============================================

# Scintillator: --scint
resolve_scint() {
    case "$1" in
        fermilab)   echo "Fermilab_scintillator.properties" ;;
        bc404)      echo "Saint-Gobain_BC-404.properties" ;;
        bc408)      echo "Saint-Gobain_BC-408.properties" ;;
        bc452-2pb)  echo "Saint-Gobain_BC-452_2perCentLead.properties" ;;
        bc452-5pb)  echo "Saint-Gobain_BC-452_5perCentLead.properties" ;;
        bc452-10pb) echo "Saint-Gobain_BC-452_10perCentLead.properties" ;;
        *)          echo "Unknown scintillator: $1" >&2; exit 1 ;;
    esac
}

# Wrapping: --wrap
resolve_wrap() {
    case "$1" in
        tio2)    echo "Wrapping_TiO2.properties" ;;
        teflon)  echo "Wrapping_Teflon.properties" ;;
        alu)     echo "Wrapping_Aluminum.properties" ;;
        bc620)   echo "Wrapping_Saint-Gobain_BC-620.properties" ;;
        tyvek)   echo "Wrapping_Tyvek.properties" ;;
        *)       echo "Unknown wrapping: $1" >&2; exit 1 ;;
    esac
}

# Twin tile reflector: --twin
resolve_twin() {
    case "$1" in
        alu) echo "TwinTileReflector_Aluminum.properties" ;;
        *)   echo "Unknown twin reflector: $1" >&2; exit 1 ;;
    esac
}

# Fiber (shared for WLS and LG): --wls, --lg
resolve_fiber() {
    case "$1" in
        # Kuraray Y11
        y11-200-r1)    echo "Kuraray_Y11-200_round_1mm.properties" ;;
        y11-200-r1-sc) echo "Kuraray_Y11-200_round_1mm_singleClad.properties" ;;
        y11-200-s1-sc) echo "Kuraray_Y11-200_square_1mm_singleClad.properties" ;;
        y11-300-r1)    echo "Kuraray_Y11-300_round_1mm.properties" ;;
        # Saint-Gobain BCF-10
        bcf10-ms1)     echo "Saint-Gobain_BCF-10_multi_square_1mm.properties" ;;
        # Saint-Gobain BCF-92 round
        bcf92-r1)      echo "Saint-Gobain_BCF-92_round_1mm.properties" ;;
        bcf92-r1-sc)   echo "Saint-Gobain_BCF-92_round_1mm_singleClad.properties" ;;
        bcf92-r2)      echo "Saint-Gobain_BCF-92_round_2mm.properties" ;;
        bcf92-r2-sc)   echo "Saint-Gobain_BCF-92_round_2mm_singleClad.properties" ;;
        # Saint-Gobain BCF-92 quadratic
        bcf92-q1)      echo "Saint-Gobain_BCF-92_quadratic_1mm.properties" ;;
        bcf92-q1-sc)   echo "Saint-Gobain_BCF-92_quadratic_1mm_singleClad.properties" ;;
        bcf92-q2)      echo "Saint-Gobain_BCF-92_quadratic_2mm.properties" ;;
        bcf92-q2-sc)   echo "Saint-Gobain_BCF-92_quadratic_2mm_singleClad.properties" ;;
        # Saint-Gobain BCF-98 round
        bcf98-r1)      echo "Saint-Gobain_BCF-98_round_1mm.properties" ;;
        bcf98-r1-sc)   echo "Saint-Gobain_BCF-98_round_1mm_singleClad.properties" ;;
        bcf98-r2)      echo "Saint-Gobain_BCF-98_round_2mm.properties" ;;
        bcf98-r2-sc)   echo "Saint-Gobain_BCF-98_round_2mm_singleClad.properties" ;;
        # Saint-Gobain BCF-98 quadratic
        bcf98-q1)      echo "Saint-Gobain_BCF-98_quadratic_1mm.properties" ;;
        bcf98-q1-sc)   echo "Saint-Gobain_BCF-98_quadratic_1mm_singleClad.properties" ;;
        bcf98-q2)      echo "Saint-Gobain_BCF-98_quadratic_2mm.properties" ;;
        bcf98-q2-sc)   echo "Saint-Gobain_BCF-98_quadratic_2mm_singleClad.properties" ;;
        # Other
        eo534b)        echo "EO-534B.properties" ;;
        *)             echo "Unknown fiber: $1" >&2; exit 1 ;;
    esac
}

# SiPM model: --sipmmodel
resolve_sipmmodel() {
    case "$1" in
        generic)                    echo "generic" ;;
        hamamatsu-s10362-11-100c)   echo "hamamatsu-s10362-11-100c" ;;
        hamamatsu-s10362-33-100c)   echo "hamamatsu-s10362-33-100c" ;;
        hamamatsu-s10362-33-050c)   echo "hamamatsu-s10362-33-050c" ;;
        hamamatsu-s12651-050)       echo "hamamatsu-s12651-050" ;;
        hamamatsu-s12573-100c)      echo "hamamatsu-s12573-100c" ;;
        hamamatsu-s12573-100x)      echo "hamamatsu-s12573-100x" ;;
        *)                          echo "$1" ;;   # pass-through for full file paths
    esac
}

# Optical cement: --cement
resolve_cement() {
    case "$1" in
        bc600)  echo "Saint-Gobain_BC-600.properties" ;;
        air)    echo "air.properties" ;;
        air1mm) echo "air_1mm_fiber.properties" ;;
        *)      echo "Unknown cement: $1" >&2; exit 1 ;;
    esac
}

# Resolve all materials
Set_Scint=$(resolve_scint "$Mat_Scint")
Set_Wrapping=$(resolve_wrap "$Mat_Wrap")
Set_TwinTileReflector=$(resolve_twin "$Mat_Twin")
Set_WLSFiber=$(resolve_fiber "$Mat_WLS")
Set_LGFiber=$(resolve_fiber "$Mat_LG")
Set_OpticalCement=$(resolve_cement "$Mat_Cement")

# ===============================================
MaterialPropertiesPath="$GODDESS/source/MaterialProperties"
GPS_Filename="inj.data"

Set_TileDims="(${Scint_Width_mm},${Scint_Thickness_mm},${Scint_Length_mm})"

# ===============================================

if ! [ -e "$OutputDirectory" ]; then
    mkdir -p "$OutputDirectory"
fi

# If a previous inj.data exists, count completed events and reduce NumberOfEvents
GPS_ControlFile="$OutputDirectory/ControlData/$GPS_Filename"
if [ -f "$GPS_ControlFile" ]; then

    CompletedEvents=$(grep -c '^EventID:' "$GPS_ControlFile")

    # Only proceed if we got a sensible integer
    if [[ "$CompletedEvents" =~ ^[0-9]+$ ]]; then

        RemainingEvents=$(( NumberOfEvents - CompletedEvents ))

        # if already done...
        if [ "$RemainingEvents" -le 0 ]; then
            echo "Detected $CompletedEvents existing events in $ExistingGPSFile,"
            echo "which is >= requested $NumberOfEvents. Nothing to do."
            exit 0
        fi

        # otherwise...
        echo "Detected $CompletedEvents existing events in $ExistingGPSFile."
        echo "Reducing NumberOfEvents from $NumberOfEvents to $RemainingEvents."
        NumberOfEvents="$RemainingEvents"
    fi
fi


InputDirectory="$OutputDirectory/Input"
if [ -e "$InputDirectory" ]; then
    rm -rf "$InputDirectory"
fi
mkdir -p "$InputDirectory"

Fname_Scinti="$MaterialPropertiesPath/Scintillator/$Set_Scint"
Fname_TwinTileReflector="$MaterialPropertiesPath/Scintillator/$Set_TwinTileReflector"
Fname_Wrapping="$MaterialPropertiesPath/Scintillator/$Set_Wrapping"
Fname_LGFiber="$MaterialPropertiesPath/Fibre/$Set_LGFiber"
Fname_WLSFiber="$MaterialPropertiesPath/Fibre/$Set_WLSFiber"
Fname_OpticalCement="$MaterialPropertiesPath/OpticalCement/$Set_OpticalCement"

# ------ write exec macro file -----
# Exec_MacFile="$OutputDirectory/Input/exec.mac"
# echo "# Macro file for a general commands." > $Exec_MacFile
# echo "/random/setSeed           $Seed" >> $Exec_MacFile
# echo "/run/setRunIDCounter      $RunID" >> $Exec_MacFile

# ------ write GPS macro file -----
GPS_MacFile="$InputDirectory/GPS.mac"
echo "# Macro file for a GeneralParticleSource injection." >> $GPS_MacFile
echo "/gps/verbose              0" >> $GPS_MacFile
echo "/gps/outFile              $GPS_Filename" >> $GPS_MacFile
echo "/gps/particle             $Inj_Particle" >> $GPS_MacFile
echo "/gps/energy/eMin          $Inj_Energy" >> $GPS_MacFile
echo "/gps/energy/eMax          $Inj_Energy" >> $GPS_MacFile
echo "/gps/plane/pos            $Inj_Center" >> $GPS_MacFile
# 
echo "/gps/plane/surfaceNormal  $Inj_Direction" >> $GPS_MacFile
echo "/gps/plane/shape          rect" >> $GPS_MacFile
echo "/gps/plane/posDist        uniform" >> $GPS_MacFile
echo "/gps/plane/a              $Inj_Rectangle_Width" >> $GPS_MacFile
echo "/gps/plane/b              $Inj_Rectangle_Length" >> $GPS_MacFile
#
echo "/gps/angle/thetaMin       $Inj_ThetaMin" >> $GPS_MacFile
echo "/gps/angle/thetaMax       $Inj_ThetaMax" >> $GPS_MacFile
echo "/gps/angle/phiMin         $Inj_PhiMin" >> $GPS_MacFile
echo "/gps/angle/phiMax         $Inj_PhiMax" >> $GPS_MacFile
echo >> $GPS_MacFile

# ------ write .init file -----
InitFile="$InputDirectory/Run.init"

if [[ $NumberOfEvents -gt 0 ]]; then
    echo "--batch $NumberOfEvents" >> $InitFile
fi
echo "--setupIDString $SetupID" >> $InitFile
if [ -n "$ParticleListFile" ]; then
    echo "--useParticleList" >> $InitFile
    echo "--particleSourceInput $ParticleListFile" >> $InitFile
else
    echo "--particleSourceInput $GPS_MacFile" >> $InitFile
fi
# echo "--macro $Exec_MacFile" >> $InitFile
#
echo "--outDir      $OutputDirectory" >> $InitFile
# echo "--outFile $Output_Filename" >> $InitFile
#
echo "--tileDims                $Set_TileDims" >> $InitFile
echo "--leadSheetThickness      $Lead_Thickness_mm" >> $InitFile
echo "--aluminumSheetThickness  $Aluminum_Thickness_mm" >> $InitFile
echo "--scinti                  $Fname_Scinti" >> $InitFile
echo "--twin                    $Fname_TwinTileReflector" >> $InitFile
echo "--wrapping                $Fname_Wrapping" >> $InitFile
echo "--lgFibre                 $Fname_LGFiber" >> $InitFile
echo "--wlsFibre                $Fname_WLSFiber" >> $InitFile
# echo "--scintiFibre $Fname_Scinti" >> $InitFile
echo "--cement                  $Fname_OpticalCement" >> $InitFile
#
if ! $TrackOpticalPhotons; then
    echo "speed up: we won't track optical photons!"
    echo "--noOpticalPhotonTracking" >> $InitFile
fi
if [ -n "$FilenamePhrase" ]; then
    echo "--add $FilenamePhrase" >> $InitFile
fi
if [ -n "$RunID" ]; then
    echo "--runID $RunID" >> $InitFile
fi
echo "--seed $Seed" >> $InitFile
if [ -n "$ManifestFile" ]; then
    echo "will build geometry from manifest: $ManifestFile"
    echo "--manifest $ManifestFile" >> $InitFile
fi
if $Quiet; then
    echo "--quiet" >> $InitFile
fi
if $UseG4Sipm; then
    echo "will use G4SiPM for photon detection"
    echo "--useG4Sipm" >> $InitFile
    if [ -n "$SipmModel" ]; then
        Set_SipmModel=$(resolve_sipmmodel "$SipmModel")
        echo "--sipmModel $Set_SipmModel" >> $InitFile
    fi
fi


# finally, run the program
InitString="--init $InitFile"
$BUILDDIR/g4scint $InitString

