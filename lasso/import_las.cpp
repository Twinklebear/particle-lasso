#include <limits>
#include <lasreader.hpp>
#include "types.h"
#include "import_las.h"

void import_las(const FileName &file_name, ParticleModel &model){
	LASreadOpener read_opener;
	read_opener.set_file_name(file_name.file_name.c_str());
	LASreader *reader = read_opener.open();
	std::cout << "Reading LIDAR data set from '" << file_name
		<< "' with " << reader->npoints << " points\n"
		<< "min = ( " << reader->get_min_x()
		<< ", " << reader->get_min_y()
		<< ", " << reader->get_min_z() << " )\n"
		<< "max = ( " << reader->get_max_x()
		<< ", " << reader->get_max_y()
		<< ", " << reader->get_max_z() << " )\n";

	bool has_color = reader->header.point_data_format == 2
		|| reader->header.point_data_format == 3
		|| reader->header.point_data_format == 5;

	auto positions = std::make_unique<DataT<float>>();
	auto colors = std::make_unique<DataT<uint8_t>>();
	positions->data.reserve(reader->npoints * 3);
	colors->data.reserve(reader->npoints);
	const float inv_max_uint16 = 1.f / std::numeric_limits<uint16_t>::max();
	size_t n_discarded = 0;
	while (reader->read_point()){
		// Points classified as low point are noise and should be discarded
		if (reader->point.get_classification() == 7){
			++n_discarded;
			continue;
		}
		reader->point.compute_coordinates();
		for (size_t i = 0; i < 3; ++i){
			positions->data.push_back(reader->point.coordinates[i]);
		}
		if (has_color){
			const uint16_t *rgba = reader->point.get_rgb();
			for (size_t i = 0; i < 4; ++i){
				colors->data.push_back(static_cast<uint8_t>(255.0 * rgba[i] * inv_max_uint16));
			}
		} else {
			for (size_t i = 0; i < 4; ++i){
				colors->data.push_back(255);
			}
		}
	}
	std::cout << "Discarded " << n_discarded << " noise classified points\n";
	model["positions"] = std::move(positions);
	model["colors"] = std::move(colors);
}

