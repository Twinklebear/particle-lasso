#include <string>
#include <algorithm>
#include <limits>
#include <fstream>
#include "import_gromacs.h"

using namespace pl;

// Original importer from @hoangthaiduong
void pl::import_gromacs(const FileName &file_name, std::vector<ParticleModel> &timesteps) {
	std::ifstream fin(file_name.c_str());

	std::cout << "Loading GROMACS File '" << file_name << "'\n";

	std::string line;
	float time = -1;
	size_t num_particles = 0;
	while (std::getline(fin, line)) {
		const bool new_timestep = starts_with(line, "Generated");
		if (new_timestep && timesteps.size() >= 32) {
			break;
		}
		if (new_timestep) {
			auto fnd = line.find("=");
			time = std::stof(line.substr(fnd + 1));
			std::cout << "Time = " << time << "\n";
			// Read number of particles
			std::getline(fin, line);
			num_particles = std::stoull(line);

			auto positions = std::make_shared<DataT<float>>();
			auto velocities = std::make_shared<DataT<float>>();
			positions->data.resize(num_particles * 3);
			velocities->data.resize(num_particles * 3);
			auto &pos = positions->data;
			auto &vel = velocities->data;

			for (size_t i = 0; i < num_particles; ++i) {
				std::getline(fin, line);
				float px, py, pz, vx, vy, vz;
				int id;
				sscanf(line.data(), "%dDZATO DZ%d %f %f %f %f %f %f", &id, &id,
						&pos[i * 3], &pos[i * 3 + 1], &pos[i * 3 + 2],
						&vel[i * 3], &vel[i * 3 + 1], &vel[i * 3 + 2]);

			}
			ParticleModel t;
			t["positions"] = positions;
			t["velocities"] = velocities;
			timesteps.push_back(t);
		}
	}
	std::cout << "Loaded " << num_particles << " particles, over "
		<< timesteps.size() << " timesteps\n";
}

