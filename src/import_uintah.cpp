#include <iostream>
#include <algorithm>
#include <stdexcept>
#include <cstdlib>
#include <cstdio>
#include <limits>
#include <tinyxml2.h>
#include "import_uintah.h"

using namespace tinyxml2;

struct UintahParticle {
	double x, y, z;
};

bool uintah_is_big_endian = false;

std::string tinyxml_error_string(const XMLError e){
	switch (e){
		case XML_NO_ATTRIBUTE:
			return "XML_NO_ATTRIBUTE";
		case XML_WRONG_ATTRIBUTE_TYPE:
			return "XML_WRONG_ATTRIBUTE_TYPE";
		case XML_ERROR_FILE_NOT_FOUND:
			return "XML_ERROR_FILE_NOT_FOUND";
		case XML_ERROR_FILE_COULD_NOT_BE_OPENED:
			return "XML_ERROR_FILE_COULD_NOT_BE_OPENED";
		case XML_ERROR_FILE_READ_ERROR:
			return "XML_ERROR_FILE_READ_ERROR";
		case XML_ERROR_ELEMENT_MISMATCH:
			return "XML_ERROR_ELEMENT_MISMATCH";
		case XML_ERROR_PARSING_ELEMENT:
			return "XML_ERROR_PARSING_ELEMENT";
		case XML_ERROR_PARSING_ATTRIBUTE:
			return "XML_ERROR_PARSING_ATTRIBUTE";
		case XML_ERROR_IDENTIFYING_TAG:
			return "XML_ERROR_IDENTIFYING_TAG";
		case XML_ERROR_PARSING_TEXT:
			return "XML_ERROR_PARSING_TEXT";
		case XML_ERROR_PARSING_CDATA:
			return "XML_ERROR_PARSING_CDATA";
		case XML_ERROR_PARSING_COMMENT:
			return "XML_ERROR_PARSING_COMMENT";
		case XML_ERROR_PARSING_DECLARATION:
			return "XML_ERROR_PARSING_DECLARATION";
		case XML_ERROR_PARSING_UNKNOWN:
			return "XML_ERROR_PARSING_UNKNOWN";
		case XML_ERROR_EMPTY_DOCUMENT:
			return "XML_ERROR_EMPTY_DOCUMENT";
		case XML_ERROR_MISMATCHED_ELEMENT:
			return "XML_ERROR_MISMATCHED_ELEMENT";
		case XML_ERROR_PARSING:
			return "XML_ERROR_PARSING";
		case XML_CAN_NOT_CONVERT_TEXT:
			return "XML_CAN_NOT_CONVERT_TEXT";
		case XML_NO_TEXT_NODE:
			return "XML_NO_TEXT_NODE";
		// XML_SUCCESS and XML_NO_ERROR both return success
		default:
			return "XML_SUCCESS";
	}
}
double ntohd(double d){
	double ret = 0;
	char *in = reinterpret_cast<char*>(&d);
	char *out = reinterpret_cast<char*>(&ret);
	for (int i = 0; i < 8; ++i){
		out[i] = in[7 - i];
	}
	return ret;
}
float ntohf(float f){
	float ret = 0;
	char *in = reinterpret_cast<char*>(&f);
	char *out = reinterpret_cast<char*>(&ret);
	for (int i = 0; i < 4; ++i){
		out[i] = in[3 - i];
	}
	return ret;
}
bool read_particles(const FileName &file_name, std::vector<float> &positions, const size_t num_particles,
				    const size_t start, const size_t end){
	// TODO: Would mmap'ing the file at the start and keeping each new file we encounter
	// mapped be faster than fopen/fread/fclose?
	FILE *fp = fopen(file_name.file_name.c_str(), "rb");
	if (!fp){
		std::cout << "Failed to open Uintah data file '" << file_name << "'\n";
		return false;
	}
	fseek(fp, start, SEEK_SET);
	size_t len = end - start;
	if (len != num_particles * sizeof(UintahParticle)){
		std::cout << "Length of data != expected length of particle data\n";
		return false;
		fclose(fp);
	}

	for (size_t i = 0; i < num_particles; ++i){
		UintahParticle p;
		if (fread(&p, sizeof(p), 1, fp) != 1){
			std::cout << "Error reading particle from file\n";
			fclose(fp);
			return false;
		}
		if (uintah_is_big_endian){
			p.x = ntohd(p.x);
			p.y = ntohd(p.y);
			p.z = ntohd(p.z);
		}
		positions.push_back(static_cast<float>(p.x));
		positions.push_back(static_cast<float>(p.y));
		positions.push_back(static_cast<float>(p.z));
	}
	fclose(fp);
	return true;
}
template<typename T>
bool read_particle_attribute(const FileName &file_name, std::vector<float> &attribs, const size_t num_particles,
							 const size_t start, const size_t end){
	// TODO: Would mmap'ing the file at the start and keeping each new file we encounter
	// mapped be faster than fopen/fread/fclose?
	FILE *fp = fopen(file_name.file_name.c_str(), "rb");
	if (!fp){
		std::cout << "Failed to open Uintah data file '" << file_name << "'\n";
		return false;
	}
	fseek(fp, start, SEEK_SET);
	size_t len = end - start;
	if (len != num_particles * sizeof(T)){
		std::cout << "Length of data != expected length of particle data\n";
		return false;
		fclose(fp);
	}

	std::vector<T> data(num_particles, T());
	if (fread(data.data(), sizeof(T), num_particles, fp) != num_particles){
		std::cout << "Error reading particle attribute from file\n";
		fclose(fp);
		return false;
	}
	fclose(fp);
	std::transform(data.begin(), data.end(), std::back_inserter(attribs),
			[](const T &t){ return static_cast<float>(t); });
	return true;
}
bool read_uintah_particle_variable(const FileName &base_path, XMLElement *elem, ParticleModel &model){
	std::string type;
	{
		const char *type_attrib = elem->Attribute("type");
		if (!type_attrib){
			std::cout << "Variable missing type attribute\n";
			return false;
		}
		type = std::string(type_attrib);
	}
	std::string variable;
	std::string file_name;
	// Note: might need uint64_t if we want to run on windows as long there is only
	// 4 bytes
	size_t index = std::numeric_limits<size_t>::max();
   	size_t start = std::numeric_limits<size_t>::max();
	size_t end = std::numeric_limits<size_t>::max();
   	size_t patch = std::numeric_limits<size_t>::max();
	size_t num_particles = 0;
	for (XMLNode *c = elem->FirstChild(); c; c = c->NextSibling()){
		XMLElement *e = c->ToElement();
		if (!e){
			std::cout << "Failed to parse Uintah variable element\n";
			return false;
		}
		std::string name = e->Value();
		const char *text = e->GetText();
		if (!text){
			std::cout << "Invalid variable '" << name << "', missing value\n";
			return false;
		}
		if (name == "variable"){
			variable = text;
		} else if (name == "filename"){
			file_name = text;
		} else if (name == "index"){
			try {
				index = std::strtoul(text, NULL, 10);
			} catch (const std::range_error &r){
				std::cout << "Invalid index value specified\n";
				return false;
			}
		}  else if (name == "start"){
			try {
				start = std::strtoul(text, NULL, 10);
			} catch (const std::range_error &r){
				std::cout << "Invalid start value specified\n";
				return false;
			}
		} else if (name == "end"){
			try {
				end = std::strtoul(text, NULL, 10);
			} catch (const std::range_error &r){
				std::cout << "Invalid end value specified\n";
				return false;
			}
		} else if (name == "patch"){
			try {
				patch = std::strtoul(text, NULL, 10);
			} catch (const std::range_error &r){
				std::cout << "Invalid patch value specified\n";
				return false;
			}
		} else if (name == "numParticles"){
			try {
				num_particles = std::strtoul(text, NULL, 10);
			} catch (const std::range_error &r){
				std::cout << "Invalid numParticles value specified\n";
				return false;
			}
		}
	}
	if (num_particles > 0){
		// Particle positions are p.x
		if (variable == "p.x"){
			if (!read_particles(base_path.join(FileName(file_name)), model["positions"], num_particles, start, end)){
				return false;
			}
		} else if (type == "ParticleVariable<double>"){
			if (!read_particle_attribute<double>(base_path.join(FileName(file_name)), model[variable],
						num_particles, start, end))
			{
				return false;
			}

		} else if (type == "ParticleVariable<float>"){
			if (!read_particle_attribute<float>(base_path.join(FileName(file_name)), model[variable],
						num_particles, start, end))
			{
				return false;
			}
		}
	}
	return true;
}
bool read_uintah_datafile(const FileName &file_name, ParticleModel &model){
	XMLDocument doc;
	XMLError err = doc.LoadFile(file_name.file_name.c_str());
	if (err != XML_SUCCESS){
		std::cout << "Error loading Uintah data file '" << file_name << "': "
			<< tinyxml_error_string(err) << "\n";
		return false;
	}
	XMLElement *node = doc.FirstChildElement("Uintah_Output");
	if (!node){
		std::cout << "No 'Uintah_Output' root XML node found in data file '"
			<< file_name << "'\n";
		return false;
	}

	const static std::string VAR_TYPE = "ParticleVariable";
	for (XMLNode *c = node->FirstChild(); c; c = c->NextSibling()){
		if (std::string(c->Value()) != "Variable"){
			std::cout << "Invalid XML node encountered, expected <Variable...>\n";
			return false;
		}
		XMLElement *e = c->ToElement();
		if (!e || !e->Attribute("type")){
			std::cout << "Invalid variable element found\n";
			return false;
		}
		std::string var_type = e->Attribute("type");
		if (var_type.substr(0, VAR_TYPE.size()) == VAR_TYPE){
			if (!read_uintah_particle_variable(file_name.path(), e, model)){
				return false;
			}
		}
	}
	return true;
}
bool read_uintah_timestep_meta(XMLNode *node){
	for (XMLNode *c = node->FirstChild(); c; c = c->NextSibling()){
		XMLElement *e = c->ToElement();
		if (!e){
			std::cout << "Error parsing Uintah timestep meta\n";
			return false;
		}
		if (std::string(e->Value()) == "endianness"){
			if (std::string(e->GetText()) == "big_endian"){
				std::cout << "Uintah parser switching to big endian\n";
				uintah_is_big_endian = true;
			}
		}
	}
	return true;
}
bool read_uintah_timestep_data(const FileName &base_path, XMLNode *node, ParticleModel &model){
	for (XMLNode *c = node->FirstChild(); c; c = c->NextSibling()){
		if (std::string(c->Value()) == "Datafile"){
			XMLElement *e = c->ToElement();
			if (!e){
				std::cout << "Error parsing Uintah timestep data\n";
				return false;
			}
			const char *href = e->Attribute("href");
			if (!href){
				std::cout << "Error parsing Uintah timestep data: Missing file href\n";
				return false;
			}
			FileName data_file = base_path.join(FileName(std::string(href)));
			if (!read_uintah_datafile(data_file, model)){
				std::cout << "Error reading Uintah data file " << data_file << "\n";
				return false;
			}
		}
	}
	return true;
}
bool read_uintah_timestep(const FileName &file_name, XMLElement *node, ParticleModel &model){
	for (XMLNode *c = node->FirstChild(); c; c = c->NextSibling()){
		if (std::string(c->Value()) == "Meta"){
			if (!read_uintah_timestep_meta(c)){
				return false;
			}
		} else if (std::string(c->Value()) == "Data"){
			if (!read_uintah_timestep_data(file_name.path(), c, model)){
				return false;
			}
		}
	}
	return true;
}

void import_uintah(const FileName &file_name, ParticleModel &model){
	std::cout << "Importing Uintah data from " << file_name << "\n";
	XMLDocument doc;
	XMLError err = doc.LoadFile(file_name.file_name.c_str());
	if (err != XML_SUCCESS){
		std::cout << "Error loading Uintah file " << tinyxml_error_string(err) << "\n";
		std::exit(1);
	}
	XMLElement *child = doc.FirstChildElement("Uintah_timestep");
	if (!child){
		std::cout << "No 'Uintah_timestep' root XML node found\n";
		std::exit(1);
	}
	if (!read_uintah_timestep(file_name, child, model)){
		std::cout << "Error reading Uintah data\n";
		std::exit(1);
	}
	float x_range[2] = { model["positions"][0], model["positions"][0] };
	float y_range[2] = { model["positions"][1], model["positions"][1] };
	float z_range[2] = { model["positions"][2], model["positions"][2] };
	std::cout << "positions count: " << model["positions"].size() << "\n";
	for (auto it = model["positions"].begin(); it != model["positions"].end();){
		if (*it < x_range[0]){
			x_range[0] = *it;
		}
		if (*it > x_range[1]){
			x_range[1] = *it;
		}
		++it;
		if (*it < y_range[0]){
			y_range[0] = *it;
		}
		if (*it > y_range[1]){
			y_range[1] = *it;
		}
		++it;
		if (*it < z_range[0]){
			z_range[0] = *it;
		}
		if (*it > z_range[1]){
			z_range[1] = *it;
		}
	}
	std::cout << "Position range from { " << x_range[0] << ", " << y_range[0]
		<< ", " << z_range[0] << " } to { " << x_range[1] << ", "
		<< y_range[1] << ", " << z_range[1] << " }\n";
	// TODO: Sort the positions, note that this also means the attributes must be re-ordered
	// in the same way so that everything still matches.
	// A super lazy possibility is to use a different internal loading data structure
	// that would be like:
	//
	// UintahDataPoint {
	//     x, y, z;
	//     attributes_hashmap
	// }
	//
	// Then sort these by position and flatten them. Performance will probably suck a bit.
}

