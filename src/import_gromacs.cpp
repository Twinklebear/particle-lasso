#include <string>
#include <algorithm>
#include <limits>
#include <fstream>
#include "import_gromacs.h"

using namespace pl;

// Original importer from @hoangthaiduong
void pl::import_gromacs(const FileName &file_name, ParticleModel &model) {
	std::ifstream fin(file_name.c_str());

	auto positions = std::make_shared<DataT<float>>();
	auto velocities = std::make_shared<DataT<float>>();

	std::cout << "Loading GROMACS File '" << file_name << "'\n";

	std::string line;
	float time = -1;
	size_t num_particles = 0;
	while (std::getline(fin, line)) {
		const bool new_timestep = starts_with(line, "Generated");
		// Only read the first timestep for now
		if (new_timestep && time != -1) {
			break;
		}
		if (new_timestep) {
			auto fnd = line.find("=");
			time = std::stof(line.substr(fnd + 1));
			std::cout << "Time = " << time << "\n";
			// Read number of particles
			std::getline(fin, line);
			num_particles = std::stoull(line);
			positions->data.reserve(num_particles * 3);
			velocities->data.reserve(num_particles * 3);
		} else if (!line.empty()) {
			float px, py, pz, vx, vy, vz;
			int id;
			sscanf(line.data(), "%dDZATO DZ%d %f %f %f %f %f %f", &id,
					&id, &px, &py, &pz, &vx, &vy, &vz);

			positions->data.push_back(px);
			positions->data.push_back(py);
			positions->data.push_back(pz);

			velocities->data.push_back(vx);
			velocities->data.push_back(vy);
			velocities->data.push_back(vz);
		}
	}
	std::cout << "Loaded " << num_particles << " particles\n";
	model["positions"] = positions;
	model["velocities"] = velocities;
}

