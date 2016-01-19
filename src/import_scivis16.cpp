#include <cassert>
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

// Compute Morton code for 3D position,
// from https://fgiesen.wordpress.com/2009/12/13/decoding-morton-codes/
uint32_t part1_by2(uint32_t x){
	if ((x & ~0x000003ff) != 0){
		std::cout << "ERROR: position data out of bounds, hash will be invalid\n";
		assert(false);
	}
	x &= 0x000003ff;                  // x = ---- ---- ---- ---- ---- --98 7654 3210
	x = (x ^ (x << 16)) & 0xff0000ff; // x = ---- --98 ---- ---- ---- ---- 7654 3210
	x = (x ^ (x <<  8)) & 0x0300f00f; // x = ---- --98 ---- ---- 7654 ---- ---- 3210
	x = (x ^ (x <<  4)) & 0x030c30c3; // x = ---- --98 ---- 76-- --54 ---- 32-- --10
	x = (x ^ (x <<  2)) & 0x09249249; // x = ---- 9--8 --7- -6-- 5--4 --3- -2-- 1--0
	return x;
}
uint32_t encode_morton3(uint32_t x, uint32_t y, uint32_t z){
	return (part1_by2(x) << 2) + (part1_by2(y) << 1) + part1_by2(z);
}

// A bin containing 32 particles
#pragma pack(1)
struct ParticleBin {
	uint32_t num_particles;
	std::array<uint64_t, 32> particles;

	ParticleBin() : num_particles(0){
		particles.fill(0);
	}
};
std::ostream& operator<<(std::ostream &os, const ParticleBin &bin){
	os << "ParticleBin {\n\tnum_particles: " << bin.num_particles
		<< "\n\tparticles: [";
	for (const auto &p : bin.particles){
		os << p << ", ";
	}
	os << "]\n}";
	return os;
}

// Header for a particle bin file. Stores information about the grid
// dimensions, offset to particle data
#pragma pack(1)
struct ParticleHeader {
	std::array<uint32_t, 3> grid_dim;
	// Number of bins stored for each grid cell
	uint32_t bins_per_cell;
	uint64_t particle_data_start;
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

	std::cout << "Saving out SciVis16 data with " << positions->data.size() / 3 << " particles"
		<< "\nPositions range from { " << x_range[0] << ", " << y_range[0]
		<< ", " << z_range[0] << " } to { " << x_range[1] << ", "
		<< y_range[1] << ", " << z_range[1] << " }\n";
	std::cout << "Concentrations range from " << concentration_range[0]
		<< " to " << concentration_range[1] << "\n";

	// We assume we're given the dimensions of the simulation, for the scivis16 data let's
	// say 10x10x10 where the range is [-5, -5, 0] to [5, 5, 10]. These dims seem roughly
	// ok since the data min/max is [-4.99, -4.99, 0] and [4.99, 4.99, 10].
	// So we have bins that are 1x1x1 and can compute which bin a particle falls into by say
	// flooring its coords.
	// TODO: If we use an ordered map the bins would be sorted in Z-order for us here, but
	// std::map performance is quite poor. Even std::unordered_map isn't so great. Maybe a
	// B-tree library somewhere?
	std::unordered_map<uint32_t, std::vector<ParticleBin>> particle_bins;
	for (uint64_t i = 0; i < positions->data.size(); i += 3){
		uint32_t x = static_cast<uint32_t>(positions->data[i] + 5.0);
		uint32_t y = static_cast<uint32_t>(positions->data[i + 1] + 5.0);
		uint32_t z = static_cast<uint32_t>(positions->data[i + 2] + 5.0);
		uint32_t b_id = encode_morton3(x, y, z);
		std::vector<ParticleBin> &bins = particle_bins[b_id];
		// If there aren't any bins or the last bin is full push on a new one
		if (bins.empty() || bins.back().num_particles == 32){
			bins.push_back(ParticleBin{});
		}
		ParticleBin &bin = bins.back();
		bin.particles[bin.num_particles++] = i / 3;
	}
	size_t max_bins = 0;
	std::cout << "# of particle bin hashes: " << particle_bins.size() << "\n";
	for (const auto &bins : particle_bins){
		std::cout << "Bin hash " << bins.first << " has " << bins.second.size()
			<< " particle bins\n";
		max_bins = std::max(max_bins, bins.second.size());
		/*
		for (const auto &b : bins.second){
			std::cout << b << "\n";
		}
		*/
	}
	std::cout << "Most bins for a hash: " << max_bins << "\n";
	// Allocate a buffer to store our file data in so we can just dump bins to it
	const size_t BIN_SIZE = sizeof(ParticleBin);
	const size_t HEADER_SIZE = sizeof(ParticleHeader);
	// TODO: For particle data we will really need adaptive storage, assuming each grid cell
	// has max_bins bins will waste a lot of memory
	// TODO: We need to compute the location to write the bins so that they're sorted in Z-order in the
	// file without having to actually do a sort. Like w/ IDX we need a way to map from the Z index to
	// the index in the sorted array
	const ParticleHeader particle_header{{10, 10, 10}, max_bins, HEADER_SIZE + (max_bins * BIN_SIZE)};

	model["positions"] = std::move(positions);
	model["velocity"] = std::move(velocity);
	model["concentration"] = std::move(concentration);
}

