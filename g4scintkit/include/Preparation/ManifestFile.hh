/*
 * ManifestFile — read/write a GeometryManifest as a flat, line-based text file.
 *
 * The format is machine-generated and machine-consumed; it is a plain element
 * list with no nesting, so no external parser library is required. Each line is
 *
 *     TYPE  key=value  key=value  ...
 *
 * where TYPE is one of SETUP, SCINT, FIBER, WRAP, SIPM, CASING. The SCINT, FIBER,
 * WRAP and SIPM lines, in file order, are the manifest's ordered placement list;
 * PlaceManifest places them in exactly that order. Blank lines and lines
 * beginning with '#' are ignored. Values contain no spaces (Write rejects any
 * that do); vectors are written as comma-separated components. All quantities
 * are in Geant4 internal units.
 *
 * Material paths
 * --------------
 * The `material=` and `glue_file=` values name GODDESS .properties files. They
 * may be given either as an absolute path, or as a path relative to the GODDESS
 * package root — e.g.
 *
 *     material=source/MaterialProperties/Scintillator/Fermilab_scintillator.properties
 *
 * Relative paths are resolved against the GODDESS environment variable (set by
 * bash_scripts/setup_paths.sh) at the point the file is used, by
 * ResolveMaterialPath below. Relative form is preferred: it makes a manifest
 * portable between machines, so it can be checked into version control and
 * shared. Read/Write store these values verbatim, so a manifest round-trips
 * byte-for-byte regardless of which form it uses.
 */

#ifndef MANIFESTFILE_HH_
#define MANIFESTFILE_HH_

#include <G4String.hh>
#include "GeometryManifest.hh"

namespace ManifestFile
{
	/// Parse a manifest file. Throws std::runtime_error on I/O or format errors.
	GeometryManifest Read(const G4String& path);

	/// Write a manifest file with full double precision (exact round-trip).
	void Write(const GeometryManifest& manifest, const G4String& path);

	/// Resolve a manifest material path (see "Material paths" above) to a path
	/// the filesystem can open. Absolute paths and the empty string are
	/// returned unchanged; a relative path is resolved against $GODDESS.
	/// Throws std::runtime_error if a relative path is given and GODDESS is
	/// unset. Call this at the point of use, never when parsing, so that the
	/// stored manifest values stay verbatim.
	G4String ResolveMaterialPath(const G4String& path);
}

#endif // MANIFESTFILE_HH_
