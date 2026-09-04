/*
 * ManifestFile — see ManifestFile.hh.
 */

#include "ManifestFile.hh"

#include <fstream>
#include <sstream>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>


namespace
{
	typedef std::map<std::string, std::string> KV;

	// ---- formatting (full precision for exact round-trip) ----

	std::string numStr(double v)
	{
		char buf[64];
		std::snprintf(buf, sizeof(buf), "%.17g", v);
		return std::string(buf);
	}

	std::string vecStr(const G4ThreeVector& v)
	{
		return numStr(v.x()) + "," + numStr(v.y()) + "," + numStr(v.z());
	}

	std::string rotStr(const double r[9])
	{
		std::string s;
		for (int i = 0; i < 9; ++i) { if (i) s += ","; s += numStr(r[i]); }
		return s;
	}

	std::string listStr(const std::vector<G4String>& xs)
	{
		std::string s;
		for (size_t i = 0; i < xs.size(); ++i) { if (i) s += ","; s += xs[i]; }
		return s;
	}

	// The flat key=value format is space-delimited, so a value must not contain
	// a space. Filesystem paths are the realistic risk; fail fast rather than
	// emit a line that Read() cannot parse back.
	void requireNoSpace(const G4String& value, const char* field)
	{
		if (std::string(value).find(' ') != std::string::npos)
			throw std::runtime_error(
				"manifest: '" + std::string(field) + "' value contains a space, "
				"which the flat manifest format cannot represent: '"
				+ std::string(value) + "'");
	}

	// ---- parsing ----

	std::string reqStr(const KV& kv, const std::string& key)
	{
		KV::const_iterator it = kv.find(key);
		if (it == kv.end())
			throw std::runtime_error("manifest: missing key '" + key + "'");
		return it->second;
	}

	std::string optStr(const KV& kv, const std::string& key)
	{
		KV::const_iterator it = kv.find(key);
		return it == kv.end() ? std::string() : it->second;
	}

	double numOf(const KV& kv, const std::string& key)
	{
		return std::strtod(reqStr(kv, key).c_str(), 0);
	}

	// Optional numeric: NaN when the key is absent. Used for fields that the
	// writer omits at their default (e.g. `start_refl` — only emitted when
	// explicitly set), so older manifests without the key still parse.
	double optNum(const KV& kv, const std::string& key)
	{
		KV::const_iterator it = kv.find(key);
		return it == kv.end() ? std::numeric_limits<double>::quiet_NaN()
		                      : std::strtod(it->second.c_str(), 0);
	}

	bool boolOf(const KV& kv, const std::string& key)
	{
		return reqStr(kv, key) == "1";
	}

	G4ThreeVector vecOf(const KV& kv, const std::string& key)
	{
		std::stringstream ss(reqStr(kv, key));
		std::string tok;
		double a[3] = {0, 0, 0};
		int i = 0;
		while (std::getline(ss, tok, ',') && i < 3) a[i++] = std::strtod(tok.c_str(), 0);
		return G4ThreeVector(a[0], a[1], a[2]);
	}

	void rotOf(const KV& kv, const std::string& key, double out[9])
	{
		std::stringstream ss(reqStr(kv, key));
		std::string tok;
		int i = 0;
		while (std::getline(ss, tok, ',') && i < 9) out[i++] = std::strtod(tok.c_str(), 0);
	}

	std::vector<G4String> listOf(const std::string& s)
	{
		std::vector<G4String> out;
		std::stringstream ss(s);
		std::string tok;
		while (std::getline(ss, tok, ',')) if (!tok.empty()) out.push_back(tok);
		return out;
	}
}


void ManifestFile::Write(const GeometryManifest& m, const G4String& path)
{
	std::ofstream f(path.c_str());
	if (!f)
		throw std::runtime_error("manifest: cannot write '" + std::string(path) + "'");

	f << "# G4ScintKit geometry manifest\n";
	f << "# units: Geant4 internal (mm, radian)\n";
	f << "# SCINT/FIBER/WRAP/SIPM lines, in file order, are the placement order\n";
	f << "# material paths are relative to the GODDESS package root ($GODDESS)\n";
	if (!m.setup_label.empty()) f << "SETUP " << m.setup_label << "\n";

	for (size_t i = 0; i < m.placements.size(); ++i)
	{
		const PlacementEntry& p = m.placements[i];
		switch (p.kind)
		{
		case PlacementEntry::SCINT:
		{
			const ScintEntry& s = p.scint;
			requireNoSpace(s.material_file, "material");
			f << "SCINT"
			  << " name=" << s.name
			  << " g4name=" << s.g4name
			  << " dims=" << vecStr(s.dims)
			  << " pos=" << vecStr(s.pos)
			  << " rot=" << rotStr(s.rot)
			  << " mother=" << s.mother
			  << " material=" << s.material_file
			  << " sensitive=" << (s.sensitive ? "1" : "0")
			  << "\n";
			break;
		}
		case PlacementEntry::FIBER:
		{
			const FiberEntry& fb = p.fiber;
			requireNoSpace(fb.material_file, "material");
			requireNoSpace(fb.glue_file, "glue_file");
			f << "FIBER"
			  << " name=" << fb.name
			  << " kind=" << fb.kind
			  << " mother=" << fb.mother
			  << " start=" << vecStr(fb.start)
			  << " end=" << vecStr(fb.end)
			  << " bend_angle=" << numStr(fb.bend_angle)
			  << " bend_axis=" << vecStr(fb.bend_axis)
			  << " material=" << fb.material_file
			  << " reference=" << fb.reference
			  << " glued=" << (fb.glued ? "1" : "0")
			  << " glue_file=" << fb.glue_file
			  << " glue_profile=" << fb.glue_profile;
			// Conditionally emitted: defaults are NaN ("not called"), and
			// omitting the token at default keeps existing baselines stable
			// while letting new designs surface the knob. `end_refl` stays
			// unconditional for backward compatibility with existing files.
			if (!std::isnan(fb.start_reflectivity))
				f << " start_refl=" << numStr(fb.start_reflectivity);
			f << " end_refl=" << numStr(fb.end_reflectivity)
			  << " loop_id=" << fb.loop_id
			  << "\n";
			break;
		}
		case PlacementEntry::WRAP:
		{
			const WrapEntry& w = p.wrap;
			requireNoSpace(w.material_file, "material");
			f << "WRAP"
			  << " scint=" << w.scint
			  << " g4name=" << w.g4name
			  << " material=" << w.material_file
			  << " cut=" << listStr(w.cut)
			  << "\n";
			break;
		}
		case PlacementEntry::SIPM:
		{
			const SipmEntry& sp = p.sipm;
			f << "SIPM"
			  << " name=" << sp.name
			  << " ref_volume=" << sp.ref_volume
			  << " face_dir=" << vecStr(sp.face_dir)
			  << " rel_pos=" << vecStr(sp.rel_pos)
			  << " edge_length=" << numStr(sp.edge_length)
			  << " fiber=" << sp.fiber
			  << " coupling_normal=" << vecStr(sp.coupling_normal)
			  << " coupling_pos=" << vecStr(sp.coupling_pos)
			  << " coupling_width=" << numStr(sp.coupling_width)
			  << " fiber_is_base=" << (sp.fiber_is_base ? "1" : "0");
			// Optional: emitted only when set, mirroring the Julia writer so
			// pre-C2 manifests stay byte-identical.
			if (!sp.model.empty()) f << " model=" << sp.model;
			f << "\n";
			break;
		}
		}
	}

	const CasingSpec& c = m.casing;
	f << "CASING"
	  << " module_half_x=" << numStr(c.module_half_x)
	  << " module_min_y=" << numStr(c.module_min_y)
	  << " module_max_y=" << numStr(c.module_max_y)
	  << " module_half_z=" << numStr(c.module_half_z)
	  << " aluminum_thickness=" << numStr(c.aluminum_thickness)
	  << " lead_thickness=" << numStr(c.lead_thickness)
	  << " num_bars=" << c.num_bars
	  << " bar_width=" << numStr(c.bar_width)
	  << " scinti_z=" << numStr(c.scinti_z)
	  << "\n";

	f.close();
}


GeometryManifest ManifestFile::Read(const G4String& path)
{
	std::ifstream f(path.c_str());
	if (!f)
		throw std::runtime_error("manifest: cannot read '" + std::string(path) + "'");

	GeometryManifest m;
	std::string line;
	int lineno = 0;

	while (std::getline(f, line))
	{
		++lineno;
		size_t p = line.find_first_not_of(" \t\r\n");
		if (p == std::string::npos) continue;       // blank
		if (line[p] == '#') continue;               // comment

		std::stringstream ss(line);
		std::string type;
		ss >> type;

		if (type == "SETUP") { ss >> m.setup_label; continue; }

		KV kv;
		std::string tok;
		while (ss >> tok)
		{
			size_t eq = tok.find('=');
			if (eq == std::string::npos)
				throw std::runtime_error("manifest: bad token '" + tok
				        + "' on line " + std::to_string(lineno));
			kv[tok.substr(0, eq)] = tok.substr(eq + 1);
		}

		if (type == "SCINT")
		{
			ScintEntry s;
			s.name          = reqStr(kv, "name");
			s.g4name        = optStr(kv, "g4name");
			s.dims          = vecOf(kv, "dims");
			s.pos           = vecOf(kv, "pos");
			rotOf(kv, "rot", s.rot);
			s.mother        = reqStr(kv, "mother");
			s.material_file = reqStr(kv, "material");
			s.sensitive     = boolOf(kv, "sensitive");
			m.placements.push_back(PlacementEntry::Scint(s));
		}
		else if (type == "FIBER")
		{
			FiberEntry fb;
			fb.name             = reqStr(kv, "name");
			fb.kind             = reqStr(kv, "kind");
			fb.mother           = reqStr(kv, "mother");
			fb.start            = vecOf(kv, "start");
			fb.end              = vecOf(kv, "end");
			fb.bend_angle       = numOf(kv, "bend_angle");
			fb.bend_axis        = vecOf(kv, "bend_axis");
			fb.material_file    = reqStr(kv, "material");
			fb.reference        = optStr(kv, "reference");
			fb.glued            = boolOf(kv, "glued");
			fb.glue_file        = optStr(kv, "glue_file");
			fb.glue_profile     = optStr(kv, "glue_profile");
			fb.start_reflectivity = optNum(kv, "start_refl");
			fb.end_reflectivity = numOf(kv, "end_refl");
			fb.loop_id          = (G4int) numOf(kv, "loop_id");
			m.placements.push_back(PlacementEntry::Fiber(fb));
		}
		else if (type == "WRAP")
		{
			WrapEntry w;
			w.scint         = reqStr(kv, "scint");
			w.g4name        = optStr(kv, "g4name");
			w.material_file = reqStr(kv, "material");
			w.cut           = listOf(optStr(kv, "cut"));
			m.placements.push_back(PlacementEntry::Wrap(w));
		}
		else if (type == "SIPM")
		{
			SipmEntry sp;
			sp.name            = reqStr(kv, "name");
			sp.ref_volume      = reqStr(kv, "ref_volume");
			sp.face_dir        = vecOf(kv, "face_dir");
			sp.rel_pos         = vecOf(kv, "rel_pos");
			sp.edge_length     = numOf(kv, "edge_length");
			sp.fiber           = reqStr(kv, "fiber");
			sp.coupling_normal = vecOf(kv, "coupling_normal");
			sp.coupling_pos    = vecOf(kv, "coupling_pos");
			sp.coupling_width  = numOf(kv, "coupling_width");
			sp.fiber_is_base   = boolOf(kv, "fiber_is_base");
			sp.model           = optStr(kv, "model");
			m.placements.push_back(PlacementEntry::Sipm(sp));
		}
		else if (type == "CASING")
		{
			CasingSpec& c = m.casing;
			c.module_half_x      = numOf(kv, "module_half_x");
			c.module_min_y       = numOf(kv, "module_min_y");
			c.module_max_y       = numOf(kv, "module_max_y");
			c.module_half_z      = numOf(kv, "module_half_z");
			c.aluminum_thickness = numOf(kv, "aluminum_thickness");
			c.lead_thickness     = numOf(kv, "lead_thickness");
			c.num_bars           = (G4int) numOf(kv, "num_bars");
			c.bar_width          = numOf(kv, "bar_width");
			c.scinti_z           = numOf(kv, "scinti_z");
		}
		else
		{
			throw std::runtime_error("manifest: unknown entry type '" + type
			        + "' on line " + std::to_string(lineno));
		}
	}

	return m;
}


G4String ManifestFile::ResolveMaterialPath(const G4String& path)
{
	// Empty is legitimate: glue_file is optional, and an empty material means
	// "let GODDESS pick its default". Pass both through untouched.
	if (path.empty()) return path;

	// Already absolute — the caller gave a full path, use it as-is. This keeps
	// manifests written before relative paths were supported working.
	if (path[0] == '/') return path;

	const char* goddess = std::getenv("GODDESS");
	if (!goddess || !*goddess)
		throw std::runtime_error(
			"manifest: material path '" + std::string(path) + "' is relative, "
			"but the GODDESS environment variable is not set, so there is "
			"nothing to resolve it against.\n"
			"  Source bash_scripts/setup_paths.sh (which exports GODDESS), or "
			"give an absolute path in the manifest.");

	std::string root(goddess);
	while (!root.empty() && root[root.size() - 1] == '/')
		root.erase(root.size() - 1);
	return G4String(root + "/" + std::string(path));
}
