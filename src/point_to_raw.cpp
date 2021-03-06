#include <cassert>
#include <iostream>
#include <algorithm>
#include <string>
#include <fstream>
#include <vector>
#include "types.h"
#include "import_las.h"
#include "import_uintah.h"
#include "import_xyz.h"
#include "import_scivis16.h"
#include "import_pkd.h"

using namespace pl;

int main(int argc, char **argv){
	if (argc < 3){
		std::cout << "Usage: point_to_raw input.(las|laz|xml|xyz|vtu|pkd) <output>.raw\n"
#if PARTICLE_LASSO_ENABLE_LIDAR
			<< "     (las|laz) - LIDAR data\n"
#endif
			<< "     xml       - Uintah data\n"
			<< "     xyz       - XYZ atomic data\n"
			<< "     pkd       - PKD data\n"
			<< "     vtu       - SciVis16 contest data\n";
		return 1;
	}
	ParticleModel model;
	std::vector<std::string> args{argv, argv + argc};
	FileName input(args[1]);
    if (input.extension() == "xml"){
		std::cout << "Converting Uintah data\n";
		import_uintah(input, model);
	} else if (input.extension() == "xyz"){
		std::cout << "Converting XYZ atomic data\n";
		import_xyz(input, model);
	} else if (input.extension() == "vtu"){
		std::cout << "Converting SciVis16 data\n";
		import_scivis16(input, model);
	} else if (input.extension() == "pkd") {
		std::cout << "Converting PKD data\n";
		import_pkd(input, model);
    }
#if PARTICLE_LASSO_ENABLE_LIDAR
    else if (input.extension() == "las" || input.extension() == "laz"){
		std::cout << "Converting LIDAR data\n";
		import_las(input, model);
	}
#endif

	if (model.empty()){
		std::cout << "Error: No data loaded\n";
		return 1;
	}
	for (const auto &d : model){
		std::cout << "Writing data " << d.first << " to '"
			<< args[2] + "_" + d.first + ".raw'\n";
		std::ofstream out(args[2] + "_" + d.first + ".raw", std::ios::binary);
		d.second->write(out);
	}
	return 0;
}

