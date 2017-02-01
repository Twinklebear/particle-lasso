#include <array>
#include <iostream>

#include <Visus/Visus.h>
#include <Visus/IdxDataset.h>

using namespace Visus;

#pragma pack(1)
struct Particle {
	float x, y, z, attrib;

	Particle(float x, float y, float z, float attrib)
		: x(x), y(y), z(z), attrib(attrib)
	{}
};

int main(int argc, const char *argv[]) {
	// Setup required junk
	UniquePtr<Application> app(new Application());
	app->init(argc, argv);
	IdxModule::attach();

	String filename = "particle_test.idx";

	// Our "field" is just a bin holding 2 particles
	Field field("particle", DType(2 * sizeof(Particle), DTypes::UINT8));
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

	// Create a dataset to open and work with the file
	auto dataset = Dataset::loadDataset(filename);
	VisusReleaseAssert(dataset && dataset->valid());
	auto access = dataset->createAccess();

	// Try to write a specific block of data to the file
	std::array<Particle, 2> particles = {
		Particle(0.0, 1.0, 1.0, 5.0),
		Particle(1.0, 2.0, 0.0, 8.0)
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
	unsigned char *data = buffer->c_ptr();
	for (size_t i = 0; i < query->getTotalNumberOfSamples(); ++i) {
		data[i] = 'a';
	}
	query->setBuffer(buffer);
	BlockQuery::execute(query);
	return 0;
}

