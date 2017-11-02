#include <cassert>
#include <algorithm>
#include <map>
#include <iostream>
#include <array>
#include <limits>
#include <unordered_map>
#include <fstream>
#include "import_scivis16.h"

template<typename T>
T clamp(T x, T lo, T hi){
	if (x < lo){
		return lo;
	}
	if (x > hi){
		return hi;
	}
	return x;
}

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
	return (part1_by2(z) << 2) | (part1_by2(y) << 1) | part1_by2(x);
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
std::ostream& operator<<(std::ostream &os, const ParticleHeader &header){
	os << "ParticleHeader {\n\tgrid_dim: {"
		<< header.grid_dim[0] << ", " << header.grid_dim[1]
		<< ", " << header.grid_dim[2] << "}\n\tbins_per_cell: "
		<< header.bins_per_cell << "\n\tparticle_data_start: "
		<< header.particle_data_start << "\n}";
	return os;
}

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

	const std::array<uint32_t, 3> grid_dim = {10, 10, 10};
	// We assume we're given the dimensions of the simulation, for the scivis16 data let's
	// say 8x8x8 where the range is [-5, -5, 0] to [5, 5, 10]. These dims seem roughly
	// ok since the data min/max is [-4.99, -4.99, 0] and [4.99, 4.99, 10].
	// So we have bins that are 1x1x1 and can compute which bin a particle falls into by say
	// flooring its coords.
	// TODO: If we use an ordered map the bins would be sorted in Z-order for us here, but
	// std::map performance is quite poor. Even std::unordered_map isn't so great. Maybe a
	// B-tree library somewhere?
	std::map<uint32_t, std::vector<ParticleBin>> particle_bins;
	for (uint64_t i = 0; i < positions->data.size() / 3; ++i){
		uint32_t x = static_cast<uint32_t>((positions->data[i * 3] + 5.0) / 10.0 * grid_dim[0]);
		uint32_t y = static_cast<uint32_t>((positions->data[i * 3 + 1] + 5.0) / 10.0 * grid_dim[1]);
		uint32_t z = static_cast<uint32_t>((positions->data[i * 3 + 2] + 5.0) / 10.0 * grid_dim[2]);
		// Clamp to the bottom-left corners of grid cells
		x = clamp(x, uint32_t{0}, grid_dim[0] - 1);
		y = clamp(y, uint32_t{0}, grid_dim[1] - 1);
		z = clamp(z, uint32_t{0}, grid_dim[2] - 1);
		uint32_t b_id = encode_morton3(x, y, z);
		std::vector<ParticleBin> &bins = particle_bins[b_id];
		// If there aren't any bins or the last bin is full push on a new one
		if (bins.empty() || bins.back().num_particles == 32){
			std::cout << "Inserting bin hash " << b_id
				<< " for pos x = " << x << ", y = " << y << ", z = " << z << "\n";
			bins.push_back(ParticleBin{});
		}
		ParticleBin &bin = bins.back();
		bin.particles[bin.num_particles++] = i;
	}
	// TODO: How can we compute the ordered index for a z index directly? I'm not
	// quite understanding the IDX computation
	std::vector<uint32_t> sorted_hashes;
	sorted_hashes.reserve(grid_dim[0] * grid_dim[1] * grid_dim[2]);
	for (uint64_t i = 0; i < grid_dim[0] * grid_dim[1] * grid_dim[2]; ++i){
		uint32_t x = i % grid_dim[0];
		uint32_t y = (i / grid_dim[0]) % grid_dim[1];
		uint32_t z = i / (grid_dim[0] * grid_dim[1]);
		sorted_hashes.push_back(encode_morton3(x, y, z));
		std::cout << "Hash " << sorted_hashes.back() << " for pos x = "
			<< x << ", y = " << y << ", z = " << z << "\n";
	}
	std::sort(sorted_hashes.begin(), sorted_hashes.end());
	std::cout << "Sorted hashes: # of them = " << sorted_hashes.size() << "\n";

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
	// TODO: For particle data we will really need adaptive storage, assuming each grid cell
	// has max_bins bins will waste a lot of memory
	// TODO: We need to compute the location to write the bins so that they're sorted in Z-order in the
	// file without having to actually do a sort. Like w/ IDX we need a way to map from the Z index to
	// the index in the sorted array
	const ParticleHeader particle_header{grid_dim, max_bins,
		sizeof(ParticleHeader) + sorted_hashes.size() * max_bins * sizeof(ParticleBin)};
	std::cout << "header = " << particle_header << "\n";
	std::ofstream fout{"test.bin", std::ios::binary};
	fout.write(reinterpret_cast<const char*>(&particle_header), sizeof(ParticleHeader));
	const ParticleBin empty_bin;
	for (const auto &z : sorted_hashes){
		std::cout << "Writing bin '" << z << "' starting at " << fout.tellp() << "\n";
		auto it = particle_bins.find(z);
		size_t remainder = max_bins;
		if (it != particle_bins.end()){
			for (const auto &b : it->second){
				std::cout << b << "\n";
				fout.write(reinterpret_cast<const char*>(&b), sizeof(ParticleBin));
				--remainder;
			}
		}
		std::cout << "filling " << remainder << " empty bins\n";
		// Fill remainder with empty bins since we expect max_bins for each hash
		for (size_t i = 0; i < remainder; ++i){
			fout.write(reinterpret_cast<const char*>(&empty_bin), sizeof(ParticleBin));
		}
		std::cout << "----------------------------------\n";
	}
	// Dump the particle data out as well. TODO: Should this be re-ordered in some way?
	// Would we want to store the data in place in the bins?
	fout.write(reinterpret_cast<char*>(positions->data.data()), sizeof(float) * positions->data.size());

	model["positions"] = std::move(positions);
	model["velocity"] = std::move(velocity);
	model["concentration"] = std::move(concentration);
}

