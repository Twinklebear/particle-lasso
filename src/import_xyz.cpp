#include <string>
#include <algorithm>
#include <limits>
#include <fstream>
#include "import_xyz.h"

using namespace pl;

void pl::import_xyz(const FileName &file_name, ParticleModel &model){
	std::ifstream file{file_name.file_name.c_str()};

	size_t num_atoms = 0;
	file >> num_atoms;
	std::string description;
	file.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
	std::getline(file, description);

	std::cout << "XYZ File '" << file_name << "'\nContains " << num_atoms
		<< " atoms\nDescription: " << description << "\n";

	auto positions = std::make_shared<DataT<float>>();
	auto atom_type = std::make_shared<DataT<int>>();
	int next_atom_id = 0;
	std::unordered_map<std::string, int> atom_type_id;

	size_t atoms_read = 0;
	std::string atom_name;
	float x, y, z;
	while (file >> atom_name >> x >> y >> z){
		if (atom_type_id.find(atom_name) == atom_type_id.end()){
			atom_type_id[atom_name] = next_atom_id++;
		}
		positions->data.push_back(x);
		positions->data.push_back(y);
		positions->data.push_back(z);
		atom_type->data.push_back(atom_type_id[atom_name]);
		++atoms_read;
	}
	if (atoms_read != num_atoms){
		std::cout << "Error reading XYZ file, got " << atoms_read << " atoms but expected "
			<< num_atoms << "\n";
	}
	for (const auto &t : atom_type_id){
		std::cout << "Atom type '" << t.first << "' id = " << t.second << "\n";
	}

	std::cout << "Read XYZ data with " << positions->data.size() / 3 << " particles\n";

	model["positions"] = std::move(positions);
	model["atom_type"] = std::move(atom_type);
}

