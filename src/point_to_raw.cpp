#include <iostream>
#include <algorithm>
#include <string>
#include <fstream>
#include <vector>
#include "types.h"
#include "import_las.h"
#include "import_uintah.h"

int main(int argc, char **argv){
	if (argc < 3){
		std::cout << "Usage: point_to_raw input.(las|laz|xml) <output>.raw\n"
			<< "    (las|laz) - LIDAR data\n"
			<< "     xml      - Uintah data\n";
		return 1;
	}
	ParticleModel model;
	std::vector<std::string> args{argv, argv + argc};
	FileName input(args[1]);
	if (input.extension() == "las" || input.extension() == "laz"){
		std::cout << "Converting LIDAR data\n";
		import_las(input, model);
	} else if (input.extension() == "xml"){
		std::cout << "Converting Uintah data\n";
		import_uintah(input, model);
	}
	if (model.empty()){
		std::cout << "Error: No data loaded\n";
		return 1;
	}
	for (const auto &d : model){
		std::cout << "Writing data " << d.first << " to '" << args[2] + "_" + d.first + ".raw'\n"
			<< "    data set contains " << d.second.size() << " floats\n";
		std::ofstream out(args[2] + "_" + d.first + ".raw", std::ios::binary);
		out.write(reinterpret_cast<const char*>(d.second.data()), sizeof(float) * d.second.size());
	}
	return 0;
}

