#include "particle_lasso.h"

using namespace pl;

std::vector<ParticleModel> pl::lasso_particles(const FileName &input) {
	std::vector<ParticleModel> timesteps;
#ifdef PARTICLE_LASSO_ENABLE_LIDAR
	if (input.extension() == "las" || input.extension() == "laz") {
		std::cout << "Importing LIDAR data\n";
		ParticleModel model;
		import_las(input, model);
		timesteps.push_back(model);
	}
#endif
	if (input.extension() == "xml") {
		std::cout << "Importing Uintah data\n";
		ParticleModel model;
		import_uintah(input, model);
		timesteps.push_back(model);
	} else if (input.extension() == "xyz") {
		std::cout << "Importing XYZ atomic data\n";
		ParticleModel model;
		import_xyz(input, model);
		timesteps.push_back(model);
	} else if (input.extension() == "vtu") {
		std::cout << "Importing SciVis16 data\n";
		ParticleModel model;
		import_scivis16(input, model);
		timesteps.push_back(model);
	} else if (input.extension() == "pkd") {
		std::cout << "Importing PKD data\n";
		ParticleModel model;
		import_pkd(input, model);
		timesteps.push_back(model);
	} else if (input.extension() == "dat") {
		std::cout << "Importing Cosmic Web data\n";
		ParticleModel model;
		import_cosmic_web(input, model);
		timesteps.push_back(model);
	} else if (input.extension() == "gro") {
		std::cout << "Importing GROMACS data\n";
		import_gromacs(input, timesteps);
	}
	return timesteps;
}

