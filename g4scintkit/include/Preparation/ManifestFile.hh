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
}

#endif // MANIFESTFILE_HH_
