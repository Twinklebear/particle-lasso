#define NOMINMAX
#include <memory>
#include <algorithm>
#include "particle_lasso.h"
#include "ospcommon/xml/XML.h"
#include "sg/common/Data.h"
#include "sg/importer/Importer.h"
#include "sg/geometry/Spheres.h"
#include "sg/common/Common.h"
#include "ospcommon/xml/XML.h"
#include "ospcommon/constants.h"

using namespace ospray::sg;

void import_particle_lasso(std::shared_ptr<Node> world, const ospcommon::FileName file_name) {
	std::cout << "Loading particles with Particle Lasso from "
		<< file_name << std::endl;

	// TODO: We need to tweak the radius based on the file (e.g., scivis contest should be
	// a lot smaller radius than 1)
	std::vector<pl::ParticleModel> timesteps = pl::lasso_particles(file_name.str());
	auto &model = timesteps[0];
	if (model.find("positions") == model.end()) {
		std::cout << "No particles in file: " << file_name << "\n";
		return;
	}
	auto geom = createNode(file_name.str(), "Spheres")->nodeAs<Spheres>();
	geom->createChild("bytes_per_sphere", "int", int(sizeof(float) * 3));
	geom->createChild("offset_center", "int", int(0));
	geom->createChild("radius", "float", 0.0015f);

	// TODO: Colormap the particles by their attribute using an sg::TransferFunction
	// then we can just store the color as an RGBA8 attribute. This is
	// probably easier/faster than fixing up my colormapped spheres module for Embree3

	auto spheres = std::make_shared<DataVector1f>();
	const pl::DataT<float> *positions = dynamic_cast<pl::DataT<float>*>(model["positions"].get());
	std::copy(positions->data.begin(), positions->data.end(),
			std::back_inserter(spheres->v));
	spheres->setName("spheres");
	geom->add(spheres);

	auto materials = geom->child("materialList").nodeAs<MaterialList>();
	materials->item(0)["d"]  = 1.f;
	materials->item(0)["Kd"] = ospcommon::vec3f(1.f);
	materials->item(0)["Ks"] = ospcommon::vec3f(0.2f);

	world->add(geom);
}

OSPSG_REGISTER_IMPORT_FUNCTION(import_particle_lasso, lasso);

#ifdef _WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

extern "C" DLLEXPORT void ospray_init_module_particle_lasso() {
	std::cout << "#particle_lasso: loading 'particle_lasso' module" << std::endl;
}

