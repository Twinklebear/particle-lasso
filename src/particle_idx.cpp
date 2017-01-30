#include <Visus/Visus.h>
#include <Visus/IdxDataset.h>

using namespace Visus;

int main(int argc, const char *argv[]) {
	// Setup required junk
	UniquePtr<Application> app(new Application());
	app->init(argc, argv);
	IdxModule::attach();

	String filename="particle_test.idx";
	//the data will be in the bounding box  p1(0,0,0) p2(15,15,15) (both p1 and p2 included)
	{
		IdxFile idxfile;
		idxfile.logic_box.setP2(NdPoint::one());
		idxfile.logic_box.setP2(0,16);
		idxfile.logic_box.setP2(1,16);
		idxfile.logic_box.setP2(2,16);
		{
			Field field("myfield",DTypes::UINT32);
			field.default_layout="";
			idxfile.fields.push_back(field);
		}
		VisusReleaseAssert(idxfile.save(filename));
	}

	//now create a Dataset, save it and reopen from disk
	auto dataset=Dataset::loadDataset(filename);
	VisusReleaseAssert(dataset && dataset->valid());

	//any time you need to read/write data from/to a Dataset I need a Access
	auto access=dataset->createAccess();

	//for example I want to write data by slices
	int cont=0;
	for (int nslice=0;nslice<16;nslice++)
	{
		//this is the bounding box of the region I'm going to write
		NdBox slice_box=dataset->getLogicBox();
		slice_box.setP1(2,nslice  );
		slice_box.setP2(2,nslice+1);

		//prepare the write query
		auto query=std::make_shared<Query>(dataset.get(),'w');
		query->setLogicPosition(slice_box);
		query->setAccess(access);
		query->begin();
		VisusReleaseAssert(!query->end() && query->getNumberOfSamples().innerProduct()==16*16);

		//fill the buffers
		auto buffer=std::make_shared<Array>(query->getNumberOfSamples(),query->getField().dtype);
		unsigned int* Dst=(unsigned int*)buffer->c_ptr();
		for (int I=0;I<16*16;I++) *Dst++=cont++;
		query->setBuffer(buffer);

		//execute the writing
		VisusReleaseAssert(query->execute());
	}
	return 0;
}

