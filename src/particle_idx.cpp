#include <array>
#include <algorithm>
#include <iostream>

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
const size_t PARTICLES_PER_SAMPLE = 2;

void test_write_block(const Field &field);
void test_read_block(const Field &field);
void test_read_box(const Field &field);

int main(int argc, const char *argv[]) {
	// Setup required junk
	UniquePtr<Application> app(new Application());
	app->init(argc, argv);
	IdxModule::attach();

	// Our "field" is just a bin holding 2 particles
	Field field("particle", DType(PARTICLES_PER_SAMPLE * sizeof(Particle), DTypes::UINT8));
	std::cout << "particle field bytes/sample = " << field.dtype.getByteSize(1) << "\n";
	field.default_layout="";
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
	std::array<Particle, PARTICLES_PER_SAMPLE * 2> particles = {
		Particle(0.0, 1.0, 1.0, 1.0),
		Particle(1.0, 2.0, 0.0, 2.0),
		Particle(0.0, 1.0, 1.0, 3.0),
		Particle(0.0, 1.0, 1.0, 4.0)
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

