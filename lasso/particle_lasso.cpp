#include "particle_lasso.h"

ParticleModel lasso_particles(const FileName &input) {
	ParticleModel model;
#ifdef PARTICLE_LASSO_ENABLE_LIDAR
	if (input.extension() == "las" || input.extension() == "laz") {
		std::cout << "Importing LIDAR data\n";
		import_las(input, model);
		return model;
	}
#endif
	if (input.extension() == "xml") {
		std::cout << "Importing Uintah data\n";
		import_uintah(input, model);
	} else if (input.extension() == "xyz") {
		std::cout << "Importing XYZ atomic data\n";
		import_xyz(input, model);
	} else if (input.extension() == "vtu") {
		std::cout << "Importing SciVis16 data\n";
		import_scivis16(input, model);
	} else if (input.extension() == "pkd") {
		std::cout << "Importing PKD data\n";
		import_pkd(input, model);
	} else if (input.extension() == "dat") {
		std::cout << "Importing Cosmic Web data\n";
		import_cosmic_web(input, model);
	}
	return model;
}

