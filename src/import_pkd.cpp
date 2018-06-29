#pragma once

#include <stdexcept>
#include "tinyxml2.h"
#include "types.h"
#include "import_pkd.h"

using namespace tinyxml2;

std::string tinyxml_error_string(const XMLError e);

void load_pkd_data(XMLNode *elem, const FileName &bin_file, ParticleModel &model) {
	std::ifstream fin(bin_file.c_str(), std::ios::binary);

	for (XMLElement *c = elem->FirstChildElement(); c; c = c->NextSiblingElement()) {
		if (std::strcmp(c->Value(), "position") == 0) {
			const size_t ofs = c->Int64Attribute("ofs");
			const size_t count = c->Int64Attribute("count");
			const std::string format = c->Attribute("format");
			if (format != "vec3f") {
				std::cout << "Error: only non-quantized PKD are supported currently\n";
				throw std::runtime_error("Unsupported PKD format");
			}
			fin.seekg(ofs);
			auto pos = std::make_shared<DataT<float>>();
			pos->data.resize(count * 3);
			const size_t read_bytes = pos->data.size() * sizeof(float);
			if (!fin.read(reinterpret_cast<char*>(pos->data.data()), read_bytes)) {
				throw std::runtime_error("Failed to read all bytes for position");
			}
			model["positions"] = std::move(pos);
		} else if (std::strcmp(c->Value(), "attribute") == 0) {
			const size_t ofs = c->Int64Attribute("ofs");
			const size_t count = c->Int64Attribute("count");
			const std::string format = c->Attribute("format");
			const std::string name = c->Attribute("name");
			if (format != "float") {
				std::cout << "Error: only float attribs are supported currently\n";
				throw std::runtime_error("Unsupported PKD attrib format");
			}
			fin.seekg(ofs);
			auto attrib = std::make_shared<DataT<float>>();
			attrib->data.resize(count);
			const size_t read_bytes = attrib->data.size() * sizeof(float);
			if (!fin.read(reinterpret_cast<char*>(attrib->data.data()), read_bytes)) {
				throw std::runtime_error("Failed to read all bytes for position");
			}
			model[name] = std::move(attrib);
		} else if (std::strcmp(c->Value(), "radius") == 0) {
			const float radius = c->FloatText();
			auto attrib = std::make_shared<DataT<float>>();
			attrib->data.push_back(radius);
			model["radius"] = std::move(attrib);
		} else if (std::strcmp(c->Value(), "useOldAlphaSpheresCode") == 0) {
			std::cout << "Ignoring use old alpha spheres command\n";
		} else {
			std::cout << "import_pkd: Unrecognized element: "
				<< c->Value() << "\n";
		}
	}
}

void import_pkd(const FileName &file_name, ParticleModel &model) {
	const FileName bin_file = file_name.path().join(FileName(file_name.name() + ".pkdbin"));
	XMLDocument doc;
	XMLError err = doc.LoadFile(file_name.c_str());
	if (err != XML_SUCCESS){
		std::cout << "Error loading PKD file " << tinyxml_error_string(err) << "\n";
		throw std::runtime_error("Failed to read PKD data");
	}

	XMLElement *node = doc.FirstChildElement("OSPRay");
	if (!node) {
		std::cout << "No OSPRay root XML node found in file\n";
		throw std::runtime_error("Failed to read PKD data");
	}
	for (XMLNode *c = node->FirstChild(); c; c = c->NextSibling()) {
		if (std::strcmp(c->Value(), "PKDGeometry") == 0) {
			load_pkd_data(c, bin_file, model);
		}
	}

	auto positions = dynamic_cast<DataT<float>*>(model["positions"].get());
	std::cout << "Read PKD data with " << positions->data.size() / 3 << " particles\n";
}

