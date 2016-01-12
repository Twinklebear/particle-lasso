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
	const vec3f min_pt = vec3f(header.GetMinX(), header.GetMinY(), header.GetMinZ())
		* scale + offset;
	const vec3f max_pt = vec3f(header.GetMaxX(), header.GetMaxY(), header.GetMaxZ())
		* scale + offset;
	std::cout << "scale: " << scale << ", offset: " << offset
		<< "\nmin = " << min_pt
		<< "\nmax = " << max_pt << "\n";

	bool has_color = header.GetDataFormatId() == liblas::ePointFormat2
		|| header.GetDataFormatId() == liblas::ePointFormat3
		|| header.GetDataFormatId() == liblas::ePointFormat5;

	auto positions = std::make_unique<DataT<float>>();
	auto colors = std::make_unique<DataT<uint8_t>>();
	positions->data.reserve(header.GetPointRecordsCount() * 3);
	colors->data.reserve(header.GetPointRecordsCount());
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
		positions->data.push_back(p.x);
		positions->data.push_back(p.y);
		positions->data.push_back(p.z);
		colors->data.push_back(static_cast<uint8_t>(c.x * 255.0));
		colors->data.push_back(static_cast<uint8_t>(c.y * 255.0));
		colors->data.push_back(static_cast<uint8_t>(c.z * 255.0));
	}
	std::cout << "Discarded " << n_discarded << " noise classified points\n"
		<< "Will save " << positions->data.size() / 3 << " points to output\n";
	model["positions"] = std::move(positions);
	model["colors"] = std::move(colors);
}

