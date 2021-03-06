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
#include "import_libbat_bpf.h"

using namespace pl;

void write_duong_vtu(const FileName &file_name, const std::shared_ptr<Data> &positions);

int main(int argc, char **argv){
	if (argc < 3){
		std::cout << "Usage: point_to_duong_vtu input.(las|laz|xml|xyz|vtu|pkd) <output>.vtu\n"
            << "Note that this is not the real VTU format, just the scivis2016 contest style with positions only\n"
#if PARTICLE_LASSO_ENABLE_LIDAR
			<< "     (las|laz) - LIDAR data\n"
#endif
			<< "     xml       - Uintah data\n"
			<< "     xyz       - XYZ atomic data\n"
			<< "     pkd       - PKD data\n"
			<< "     vtu       - SciVis16 contest data\n"
			<< "     bpf       - libbat binary particle files\n";
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
    } else if (input.extension() == "bpf") {
		std::cout << "Converting libbat BPF data\n";
		import_libbat_bpf(input, model);
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

    write_duong_vtu(args[2], model["positions"]);
	return 0;
}

struct DuongVTUHeader {
	int32_t _pad3;
	int32_t size;
	int32_t _pad1;
	int32_t step;
	int32_t _pad2;
	float time;
};

void write_duong_vtu(const FileName &file_name, const std::shared_ptr<Data> &positions){
	// Offset from the example code of reading the data
	const size_t MAGIC_OFFSET = 4072;
    const std::vector<char> padding(MAGIC_OFFSET, 0);

	std::ofstream file{file_name.file_name.c_str(), std::ios::binary};
    file.write(padding.data(), padding.size());

	DuongVTUHeader header;
    header.size = positions->size() / 3;
    header.step = 0;
    header.time = 0.f;
	std::cout << "Writing out " << header.size << " particles\n";

    file.write(reinterpret_cast<char*>(&header), sizeof(header));

    // Advance 4 bytes to match the scivs2016 layout
    file.write(reinterpret_cast<char*>(&header.step), 4);
    // Write the particle data
    positions->write(file);
}


