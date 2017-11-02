#include <cassert>
#include <algorithm>
#include <map>
#include <iostream>
#include <array>
#include <limits>
#include <unordered_map>
#include <fstream>
#include "import_scivis16.h"

struct Header {
	int32_t _pad3;
	int32_t size;
	int32_t _pad1;
	int32_t step;
	int32_t _pad2;
	float time;
};

// This follows the reading example shown in code/max_concentration.cpp that's
// included in the SciVis16 data download
void import_scivis16(const FileName &file_name, ParticleModel &model){
	std::cout << "Reading SciVis16 data from '" << file_name << "'\n";

	// Offset from the example code of reading the data
	const size_t MAGIC_OFFSET = 4072;
	std::ifstream file{file_name.file_name.c_str(), std::ios::binary};
	file.seekg(MAGIC_OFFSET);

	Header header;
	file.read(reinterpret_cast<char*>(&header), sizeof(header));
	std::cout << "File contains " << header.size << " particles for timestep "
		<< header.step << "\n";

	auto positions = std::make_unique<DataT<float>>();
	positions->data.resize(header.size * 3);
	auto velocity = std::make_unique<DataT<float>>();
	velocity->data.resize(header.size * 3);
	auto concentration = std::make_unique<DataT<float>>();
	concentration->data.resize(header.size);

	file.seekg(4, std::ios_base::cur);
	file.read(reinterpret_cast<char*>(positions->data.data()),
			positions->data.size() * sizeof(float));
	file.seekg(4, std::ios_base::cur);
	file.read(reinterpret_cast<char*>(velocity->data.data()),
			velocity->data.size() * sizeof(float));
	file.seekg(4, std::ios_base::cur);
	file.read(reinterpret_cast<char*>(concentration->data.data()),
			concentration->data.size() * sizeof(float));

	float x_range[2] = { std::numeric_limits<float>::max(), std::numeric_limits<float>::lowest() };
	float y_range[2] = { std::numeric_limits<float>::max(), std::numeric_limits<float>::lowest() };
	float z_range[2] = { std::numeric_limits<float>::max(), std::numeric_limits<float>::lowest() };

	for (size_t i = 0; i < positions->data.size(); i += 3){
		x_range[0] = std::min(x_range[0], positions->data[i]);
		x_range[1] = std::max(x_range[1], positions->data[i]);

		y_range[0] = std::min(y_range[0], positions->data[i + 1]);
		y_range[1] = std::max(y_range[1], positions->data[i + 1]);

		z_range[0] = std::min(z_range[0], positions->data[i + 2]);
		z_range[1] = std::max(z_range[1], positions->data[i + 2]);
	}

	float concentration_range[2] = { std::numeric_limits<float>::max(), std::numeric_limits<float>::lowest() };
	for (size_t i = 0; i < concentration->data.size(); ++i){
		concentration_range[0] = std::min(x_range[0], concentration->data[i]);
		concentration_range[1] = std::max(x_range[1], concentration->data[i]);
	}

	model["positions"] = std::move(positions);
	model["velocity"] = std::move(velocity);
	model["concentration"] = std::move(concentration);
}

