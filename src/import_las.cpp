#include "types.h"
#include "import_las.h"

void import_las(const FileName &file_name, ParticleModel &model){
	std::ifstream fin(file_name.file_name.c_str(), std::ios::in | std::ios::binary);
	liblas::ReaderFactory rf;
	liblas::Reader reader = rf.CreateWithStream(fin);

	const liblas::Header &header = reader.GetHeader();
	std::cout << "Reading LIDAR data set from " << file_name
		<< " with " << header.GetPointRecordsCount()
		<< " points\n";
	//Offsets we need to apply to position the points properly
	const vec3f scale = vec3f(header.GetScaleX(), header.GetScaleY(), header.GetScaleZ());
	const vec3f offset = vec3f(header.GetOffsetX(), header.GetOffsetY(), header.GetOffsetZ());
	std::cout << "scale: " << scale << ", offset: " << offset << std::endl;

	bool has_color = header.GetDataFormatId() == liblas::ePointFormat2
		|| header.GetDataFormatId() == liblas::ePointFormat3
		|| header.GetDataFormatId() == liblas::ePointFormat5;

	auto &positions = model["positions"];
	auto &colors = model["colors"];
	positions.reserve(header.GetPointRecordsCount() * 3);
	colors.reserve(header.GetPointRecordsCount());
	int n_discarded = 0;
	const float inv_max_color = 1.f / 65536.f;
	// Unfortunately the libLAS designers prefer Java style iterators
	while (reader.ReadNextPoint()){
		const liblas::Point &lasp = reader.GetPoint();
		// Points classified as low point are noise and should be discarded
		if (lasp.GetClassification().GetClass() == liblas::Point::eLowPoint){
			++n_discarded;
			continue;
		}
		const liblas::Color lasc = lasp.GetColor();
		const vec3f p = vec3f(lasp.GetX(), lasp.GetY(), lasp.GetZ()) * scale + offset;
		vec3f c;
		if (has_color){
			c = vec3f(lasc.GetRed() * inv_max_color, lasc.GetGreen() * inv_max_color,
					lasc.GetBlue() * inv_max_color);
		}
		else {
			c = vec3f(1.0);
		}
		positions.push_back(p.x);
		positions.push_back(p.y);
		positions.push_back(p.z);
		colors.push_back(c.x);
		colors.push_back(c.y);
		colors.push_back(c.z);
	}
	std::cout << "Discarded " << n_discarded << " noise classified points\n"
		<< "Will save " << positions.size() / 3 << " points to output\n";
}

