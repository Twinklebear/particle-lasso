#include <array>
#include <unordered_map>
#include <algorithm>
#include <iostream>
#include <string>
#include <limits>
#include <fstream>
#include <random>

#include <Visus/Visus.h>
#include <Visus/IdxDataset.h>

using namespace Visus;

// The Visus IDX reader doesn't tell you if it failed to read a block due
// to it being missing in the file and instead just returns 0 filled buffer
// for those values. As a temp workaround I could put a non-zero header on
// the particle to check if the particle actually exists or not.
#pragma pack(1)
struct Particle {
	// Temporary workaround tag to mark particles as valid or not
	// since Visus doesn't tell us if a read failed and will just
	// return a 0-filled region for the missing data.
	int valid;
	float x, y, z, attrib;

	Particle() : valid(0), x(0), y(0), z(0), attrib(0) {}
	Particle(float x, float y, float z, float attrib)
		: valid(1), x(x), y(y), z(z), attrib(attrib)
	{}
};
std::ostream& operator<<(std::ostream &os, const Particle &p) {
	os << " Particle{(" << p.x << ", " << p.y << ", " << p.z << "), "
		<< p.attrib << " }";
	return os;
}

const String filename = "particle_test.idx";
size_t PARTICLES_PER_SAMPLE = 2;

void test_write_block(const Field &field);
void test_read_block(const Field &field);
void test_read_box(const Field &field);
void convert_xyz(const std::string &fname, const std::string &out_name, const Field &field,
		const size_t bits_per_block, const std::array<size_t, 3> &idx_grid_dims, const bool randomize);
inline float scale_range(const float x, const float old_min, const float old_max,
		const float new_min, const float new_max)
{
	return (new_max - new_min) * (x - old_min) / (old_max - old_min) + new_min;
}

int main(int argc, const char *argv[]) {
	// Setup required junk
	UniquePtr<Application> app(new Application());
	app->init(argc, argv);
	IdxModule::attach();

	std::string xyz_fname, out_name;
	size_t bits_per_block = 2;
	std::array<size_t, 3> idx_grid_dims{4, 4, 4};
	bool randomize = false;
	for (int i = 1; i < argc; ++i) {
		if (std::strcmp(argv[i], "-pps") == 0) {
			PARTICLES_PER_SAMPLE = std::atoi(argv[++i]);
		} else if (std::strcmp(argv[i], "-xyz") == 0) {
			xyz_fname = argv[++i];
		} else if (std::strcmp(argv[i], "-o") == 0) {
			out_name = argv[++i];
		} else if (std::strcmp(argv[i], "-bpb") == 0) {
			bits_per_block = std::atol(argv[++i]);
		} else if (std::strcmp(argv[i], "-grid") == 0) {
			for (auto &d : idx_grid_dims) {
				d = std::atol(argv[++i]);
			}
		} else if (std::strcmp(argv[i], "-rand") == 0) {
			randomize = true;
		}
	}
	std::cout << "Will store " << PARTICLES_PER_SAMPLE << " particles per sample\n";

	// Our "field" is just a bin holding 2 particles
	Field field("particle", DType(PARTICLES_PER_SAMPLE * sizeof(Particle), DTypes::UINT8));
	std::cout << "particle field bytes/sample = " << field.dtype.getByteSize(1) << "\n";
	field.default_layout="";

	if (!xyz_fname.empty()) {
		if (out_name.empty()) {
			std::cout << "an output filename via -o is also required\n";
			return 1;
		}
		convert_xyz(xyz_fname, out_name, field, bits_per_block, idx_grid_dims, randomize);
	} else {
		std::cout << "Generating and testing basic IDX particle file\n";
		// This 4^2 image defines our virtual fine-level grid that we're placing
		// over the dataset to bin the particles
		{
			IdxFile idxfile;
			idxfile.bitsperblock = 1;
			idxfile.logic_box.setP2(NdPoint::one());
			idxfile.logic_box.setP2(0, 4);
			idxfile.logic_box.setP2(1, 4);
			idxfile.fields.push_back(field);
			VisusReleaseAssert(idxfile.save(filename));
		}
		test_write_block(field);
		test_read_block(field);
		test_read_box(field);
	}
	return 0;
}
void test_write_block(const Field &field) {
	std::cout << "Testing Write Block\n";
	// Create a dataset to open and work with the file
	auto dataset = Dataset::loadDataset(filename);
	VisusReleaseAssert(dataset && dataset->valid());
	auto access = dataset->createAccess();

	// Try to write a specific block of data to the file, containing 2 samples
	const size_t samples_per_block = 1 << dataset->getDefaultBitsPerBlock();
	std::cout << "samples per block = " << samples_per_block << "\n";
	VisusReleaseAssert(samples_per_block == 2);
	// samples per block should be 2
	std::vector<Particle> particles = {
		Particle(0.0, 1.0, 1.0, 1.0),
		Particle(1.0, 2.0, 0.0, 2.0),
		Particle(0.0, 1.5, 1.0, 3.0),
		Particle(0.5, 0.5, 1.0, 4.0)
	};

	auto query = std::make_shared<BlockQuery>(dataset.get(), 'w');
	query->setAccess(access);
	query->setField(field);
	// These are the (global?) Z-indices of the samples we want to write,
	// the range queried is [start, end).
	query->setStartAddress(8);
	query->setEndAddress(10);

	std::cout << "start addr = " << query->getStartAddress()
		<< "\nend addr = " << query->getEndAddress()
		<< std::endl;

	query->begin();
	VisusReleaseAssert(!query->end());

	auto &samples = query->getSamples();
	// How many samples should be here? For a block query it's required that we write
	// the entire block, and the total number of samples returned to us may not be right,
	// since it won't account for this level possibly not filling the block or being
	// split across blocks.
	std::cout << "total # of samples = " << query->getTotalNumberOfSamples() << "\n"
		<< "query samples inner prod = " << samples.nsamples.innerProduct() << "\n"
		// The logic box may have its end point out of bounds due to the level delta (stride)
		// so we should just clamp the end point to the edge. Besides that it's like
		// the other logic boxes in that it specifies an inclusive n-D box.
		<< "samples logic box = " << samples.logic_box.toString() << std::endl;

	// TODO: This doesn't seem to give us the right buffer size, we need to write whole blocks with this
	// but total number of samples just refers to our query, not the entire block size.
	auto buffer = std::make_shared<Array>(query->getTotalNumberOfSamples(), query->getField().dtype);
	std::cout << "buffer size = " << buffer->c_size() << "\n";
	unsigned char *data = buffer->c_ptr();
	std::memcpy(data, particles.data(), sizeof(Particle) * particles.size());
	query->setBuffer(buffer);
	// Now test reading back the particle data
	Particle *p = reinterpret_cast<Particle*>(buffer->c_ptr());
	for (size_t i = 0; i < PARTICLES_PER_SAMPLE * samples.nsamples.innerProduct(); ++i) {
		std::cout << "particles written[" << i << "] = " << p[i] << std::endl;
	}
	BlockQuery::executeAndWait(query);
	std::cout << "-------" << std::endl;
}
void test_read_block(const Field &field) {
	std::cout << "Testing Read Block\n";
	// Create a dataset to open and work with the file
	auto dataset = Dataset::loadDataset(filename);
	VisusReleaseAssert(dataset && dataset->valid());
	auto access = dataset->createAccess();

	// TODO: What we really want to do are box queries on the data for LOD queries
	// So interpolating the samples makes no sense, we want to just get the existing samples
	// and not fill in or interpolate missing space b/c these data are particles not voxel
	// samples.
	auto query = std::make_shared<BlockQuery>(dataset.get(), 'r');
	query->setAccess(access);
	query->setField(field);
	// These are the (global?) Z-indices of the samples we want to write,
	// the range queried is [start, end).
	query->setStartAddress(8);
	query->setEndAddress(10);

	std::cout << "start addr = " << query->getStartAddress()
		<< "\nend addr = " << query->getEndAddress()
		<< std::endl;

	query->begin();
	VisusReleaseAssert(!query->end());

	auto &samples = query->getSamples();
	// How many samples should be here? For a block query it's required that we write
	// the entire block, and the total number of samples returned to us may not be right,
	// since it won't account for this level possibly not filling the block or being
	// split across blocks.
	std::cout << "total # of samples = " << query->getTotalNumberOfSamples() << "\n"
		<< "query samples inner prod = " << samples.nsamples.innerProduct() << "\n"
		// The logic box may have its end point out of bounds due to the level delta (stride)
		// so we should just clamp the end point to the edge. Besides that it's like
		// the other logic boxes in that it specifies an inclusive n-D box.
		<< "samples logic box = " << samples.logic_box.toString() << "\n"
		<< "samples logic box p1 = " << samples.logic_box.p1().toString() << "\n"
		<< "samples logic box p2 = " << samples.logic_box.p2().toString() << std::endl;

	BlockQuery::executeAndWait(query);
	auto buffer = query->getBuffer();
	std::cout << "buffer size = " << buffer->c_size() << "\n";

	// Now test reading back the particle data
	Particle *particles = reinterpret_cast<Particle*>(buffer->c_ptr());
	for (size_t i = 0; i < PARTICLES_PER_SAMPLE * samples.nsamples.innerProduct(); ++i) {
		std::cout << "particles read[" << i << "] = " << particles[i] << std::endl;
	}
	std::cout << "------" << std::endl;
}
void test_read_box(const Field &field) {
	std::cout << "Testing Read Box\n";
	// Create a dataset to open and work with the file
	auto dataset = Dataset::loadDataset(filename);
	VisusReleaseAssert(dataset && dataset->valid());
	auto access = dataset->createAccess();

	const NdBox world_box = dataset->getLogicBox();
	std::cout << "World box = " << world_box.toString() << "\n"
		<< "world box p1 = " << world_box.p1().toString() << "\n"
		<< "world box p2 = " << world_box.p2().toString() << "\n";
	NdBox query_box = world_box;
	query_box.setP1(NdPoint(0, 1, 0, 0, 0));
	query_box.setP2(NdPoint(2, 3, 1, 1, 1));
	std::cout << "Query box = " << query_box.toString() << "\n"
		<< "query box p1 = " << query_box.p1().toString() << "\n"
		<< "query box p2 = " << query_box.p2().toString() << "\n";

	// TODO: What we really want to do are box queries on the data for LOD queries
	// So interpolating the samples makes no sense, we want to just get the existing samples
	// and not fill in or interpolate missing space b/c these data are particles not voxel
	// samples.
	auto query = std::make_shared<Query>(dataset.get(), 'r');
	query->setAccess(access);
	query->setField(field);
	query->setLogicPosition(query_box);
	// Is it ok if I query the whole thing when only one block actually exists?
	// What does end resolution mean? What do I get if I add none of this? Do I get
	// all of the samples?
	query->addEndResolution(dataset->getMaxResolution());

	// We don't want to interpolate samples b/c they're not voxels or pixels that
	// you can interpolate but particle bins
	query->setMergeMode(Query::InsertSamples);
	query->begin();

	auto &samples = query->getSamples();
	// How many samples should be here? For a block query it's required that we write
	// the entire block, and the total number of samples returned to us may not be right,
	// since it won't account for this level possibly not filling the block or being
	// split across blocks.
	std::cout << "query samples inner prod = " << samples.nsamples.innerProduct() << "\n"
		// The logic box may have its end point out of bounds due to the level delta (stride)
		// so we should just clamp the end point to the edge. Besides that it's like
		// the other logic boxes in that it specifies an inclusive n-D box.
		<< "samples logic box = " << samples.logic_box.toString() << std::endl;

	query->execute();

	auto buffer = query->getBuffer();
	std::cout << "buffer size = " << buffer->c_size() << "\n";

	// Now test reading back the particle data
	Particle *particles = reinterpret_cast<Particle*>(buffer->c_ptr());
	const size_t possible_particles = PARTICLES_PER_SAMPLE * samples.nsamples.innerProduct();
	auto pend = std::partition(particles, particles + possible_particles,
			[](const Particle &p) { return p.valid != 0; });
	std::cout << "valid particles in the file = " << std::distance(particles, pend) << "\n";
	for (auto it = particles; it != pend; ++it) {
		std::cout << "particles read = " << *it << "\n";
	}
	std::cout << "------" << std::endl;
}
void convert_xyz(const std::string &fname, const std::string &out_name, const Field &field,
		const size_t bits_per_block, const std::array<size_t, 3> &idx_grid_dims, const bool randomize)
{
	std::ifstream file(fname.c_str());

	size_t num_atoms = 0;
	file >> num_atoms;
	std::string description;
	file.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
	std::getline(file, description);

	std::cout << "XYZ File '" << fname << "'\nContains " << num_atoms
		<< " atoms\nDescription: " << description << "\n";

	std::vector<Particle> particles;
	int next_atom_id = 0;
	std::unordered_map<std::string, int> atom_type_id;

	float x_range[2] = { std::numeric_limits<float>::max(), std::numeric_limits<float>::lowest() };
	float y_range[2] = { std::numeric_limits<float>::max(), std::numeric_limits<float>::lowest() };
	float z_range[2] = { std::numeric_limits<float>::max(), std::numeric_limits<float>::lowest() };

	size_t atoms_read = 0;
	std::string atom_name;
	float x, y, z;
	while (file >> atom_name >> x >> y >> z){
		if (atom_type_id.find(atom_name) == atom_type_id.end()){
			atom_type_id[atom_name] = next_atom_id++;
		}
		particles.push_back(Particle(x, y, z, atom_type_id[atom_name]));
		x_range[0] = std::min(x_range[0], x);
		x_range[1] = std::max(x_range[1], x);
		y_range[0] = std::min(y_range[0], y);
		y_range[1] = std::max(y_range[1], y);
		z_range[0] = std::min(z_range[0], z);
		z_range[1] = std::max(z_range[1], z);
		++atoms_read;
	}
	if (atoms_read != num_atoms){
		std::cout << "Error reading XYZ file, got " << atoms_read << " atoms but expected "
			<< num_atoms << "\n";
	}
	for (const auto &t : atom_type_id){
		std::cout << "Atom type '" << t.first << "' id = " << t.second << "\n";
	}
	// TODO: We need to setup the logic to physic box to apply this transformation
	std::cout << "XYZ position file range = {" << x_range[0]
		<< ", " << y_range[0] << ", " << z_range[0] << "} to {"
		<< x_range[1] << ", " << y_range[1] << ", " << z_range[1]
		<< "}\n";

	// Make the IDX file we'll store the particles in
	const size_t samples_per_block = 1 << bits_per_block;
	std::cout << "Using " << bits_per_block << " bits per block, with "
		<< samples_per_block << " samples per block\n"
		<< "IDX Grid dimensions = {" << idx_grid_dims[0]
		<< ", " << idx_grid_dims[1] << ", " << idx_grid_dims[2]
		<< "}\n";
	// Setup the transform from physical space (the particle value range) to
	// logical space (the IDX grid dimensions)
	Matrix physic_to_logic = Matrix::identity();
	// NOTE: I offset down by a tiny byt since the Visus box contains operation doesn't
	// include its upper edge and thus we have some data which lies on those faces which is
	// counted as outside incorrectly.
	physic_to_logic(0, 0) = (idx_grid_dims[0] - 0.25) / (x_range[1] - x_range[0]);
	physic_to_logic(0, 3) = -x_range[0] * (idx_grid_dims[0] - 0.25) / (x_range[1] - x_range[0]);

	physic_to_logic(1, 1) = (idx_grid_dims[1] - 0.25) / (y_range[1] - y_range[0]);
	physic_to_logic(1, 3) = -y_range[0] * (idx_grid_dims[1] - 0.25) / (y_range[1] - y_range[0]);

	physic_to_logic(2, 2) = (idx_grid_dims[2] - 0.25) / (z_range[1] - z_range[0]);
	physic_to_logic(2, 3) = -z_range[0] * (idx_grid_dims[2] - 0.25) / (z_range[1] - z_range[0]);
	std::cout << "physic to logic = " << physic_to_logic.toString() << "\n";
	// Create the IDX file on disk
	{
		IdxFile idxfile;
		idxfile.bitsperblock = bits_per_block;
		idxfile.logic_box.setP2(NdPoint::one());
		idxfile.logic_box.setP2(0, idx_grid_dims[0]);
		idxfile.logic_box.setP2(1, idx_grid_dims[1]);
		idxfile.logic_box.setP2(2, idx_grid_dims[2]);
		idxfile.logic_to_physic = physic_to_logic.invert();
		idxfile.fields.push_back(field);
		VisusReleaseAssert(idxfile.save(out_name));
	}

	if (randomize) {
		std::cout << "Particles will be sampled randomly from the dataset\n";
	}

	auto dataset = Dataset::loadDataset(out_name);
	std::cout << "logic to physic = " << dataset->getLogicToPhysic().toString() << "\n";
	if (!dataset || !dataset->valid()) {
		throw std::runtime_error("Failed to open dataset");
	}
	auto access = dataset->createAccess();

	std::random_device rd;
	std::mt19937_64 rng(rd());

	// TODO: In the future this would not be true, as we may need to refine
	// some regions if there's a lot of particle data there. Then the number
	// of blocks would be changing.
	const size_t num_blocks = idx_grid_dims[0] * idx_grid_dims[1] * idx_grid_dims[2] / samples_per_block;
	std::cout << "IDX file will have " << num_blocks << " blocks without adaptive regions\n";
	// Write to each block
	for (size_t i = 0; i < num_blocks; ++i) {
		std::cout << "------\nBlock query for block " << i << "\n";
		auto query = std::make_shared<BlockQuery>(dataset.get(), 'w');
		query->setAccess(access);
		query->setField(field);
		// These are the (global?) Z-indices of the samples we want to write,
		// the range queried is [start, end).
		query->setStartAddress(i * samples_per_block);
		query->setEndAddress((i + 1) * samples_per_block);
		std::cout << "start addr = " << query->getStartAddress()
			<< "\nend addr = " << query->getEndAddress()
			<< std::endl;

		query->begin();
		if (query->end()) {
			throw std::runtime_error("Query has ended incorrectly, error with query");
		}

		auto &samples = query->getSamples();
		// How many samples should be here? For a block query it's required that we write
		// the entire block, and the total number of samples returned to us may not be right,
		// since it won't account for this level possibly not filling the block or being
		// split across blocks.
		std::cout << "total # of samples = " << query->getTotalNumberOfSamples() << "\n"
			<< "query samples inner prod = " << samples.nsamples.innerProduct() << "\n"
			// The logic box may have its end point out of bounds due to the level delta (stride)
			// so we should just clamp the end point to the edge. Besides that it's like
			// the other logic boxes in that it specifies an inclusive n-D box.
			<< "samples logic box = " << samples.logic_box.toString() << std::endl;
		if (query->getTotalNumberOfSamples() != samples_per_block) {
			throw std::runtime_error("Invalid # of samples, not equal to samples per block");
		}

		auto buffer = std::make_shared<Array>(query->getTotalNumberOfSamples(), query->getField().dtype);
		std::cout << "buffer size = " << buffer->c_size() << "\n";
		std::memset(buffer->c_ptr(), 0, buffer->c_size());
		Particle *data = reinterpret_cast<Particle*>(buffer->c_ptr());
		size_t particles_for_block = 0;
		if (randomize) {
			std::shuffle(particles.begin(), particles.end(), rng);
		}
		// Try to find some particles for this block.
		for (size_t j = 0; j < particles.size();) {
			// The boxes are not inclusive on their upper edge, so nudge things down a little bit
			const Point4d lpos = physic_to_logic * Point4d(particles[j].x, particles[j].y, particles[j].z, 1.0);
			const NdPoint p(lpos.x, lpos.y, lpos.z);
#if 0
			std::cout << "particle " << particles[j] << " transformed pt to "
				<< p.toString() << " in logic space\n";
#endif
			if (samples.logic_box.containsPoint(p)) {
				//std::cout << "particle is in this block\n";
				data[particles_for_block++] = particles[j];
				particles.erase(particles.begin() + j);
				if (particles_for_block == query->getTotalNumberOfSamples() * PARTICLES_PER_SAMPLE) {
					std::cout << "This block is filled with " << particles_for_block << " particles\n";
					break;
				}
			} else {
				++j;
			}
		}
		if (particles_for_block == 0) {
			std::cout << "block is empty\n";
		}

		query->setBuffer(buffer);
		BlockQuery::execute(query);

		std::cout << "------\n";
		if (particles.empty()) {
			std::cout << "All particles written, exiting\n";
			break;
		}
	}
	if (!particles.empty()) {
		std::cout << "There were " << particles.size() << " left over particles of the total "
			<< num_atoms << " that weren't written."
			<< " Likely region refinement is required to subdivide the higher density areas further\n"
			<< "Left over particles:\n";
		/*
		for (const auto &p : particles) {
			std::cout << p << "\n";
		}
		*/
	}
}

