#include <memory>
#include <iostream>
#include <vector>
#include <unordered_map>

#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <imgui.h>

#include <vl/glt/buffer_allocator.h>
#include <vl/plugin.h>
#include <vl/glt/util.h>

#include <Visus/Visus.h>
#include <Visus/IdxDataset.h>

static const size_t PARTICLES_PER_SAMPLE = 2;

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

struct ParticleDataset {
	Visus::SharedPtr<Visus::Dataset> dataset;
	std::vector<Particle> particles;
	glt::SubBuffer vbo;

	ParticleDataset(const vl::FileName &fname);
};
ParticleDataset::ParticleDataset(const vl::FileName &fname)
	: dataset(Visus::Dataset::loadDataset(fname.file_name))
{
	using namespace Visus;

	// Our "field" is just a bin holding 2 particles
	Field field("particle", DType(PARTICLES_PER_SAMPLE * sizeof(Particle), DTypes::UINT8));
	std::cout << "particle field bytes/sample = " << field.dtype.getByteSize(1) << "\n";
	field.default_layout="";

	if (!dataset || !dataset->valid()) {
		throw std::runtime_error("Failed to load IDX dataset");
	}
	auto access = dataset->createAccess();

	const NdBox world_box = dataset->getLogicBox();
	std::cout << "World box = " << world_box.toString() << "\n"
		<< "world box p1 = " << world_box.p1().toString() << "\n"
		<< "world box p2 = " << world_box.p2().toString() << "\n";
	/*
	NdBox query_box = world_box;
	query_box.setP1(NdPoint(0, 1, 0, 0, 0));
	query_box.setP2(NdPoint(2, 3, 1, 1, 1));
	std::cout << "Query box = " << query_box.toString() << "\n"
		<< "query box p1 = " << query_box.p1().toString() << "\n"
		<< "query box p2 = " << query_box.p2().toString() << "\n";
	*/

	// TODO: What we really want to do are box queries on the data for LOD queries
	// So interpolating the samples makes no sense, we want to just get the existing samples
	// and not fill in or interpolate missing space b/c these data are particles not voxel
	// samples.
	auto query = std::make_shared<Query>(dataset.get(), 'r');
	query->setAccess(access);
	query->setField(field);
	query->setLogicPosition(world_box);
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
	Particle *p_data = reinterpret_cast<Particle*>(buffer->c_ptr());
	const size_t possible_particles = PARTICLES_PER_SAMPLE * samples.nsamples.innerProduct();
	std::copy_if(p_data, p_data + possible_particles, std::back_inserter(particles),
			[](const Particle &p) { return p.valid != 0; });
	std::cout << "valid particles in the file = " << particles.size() << "\n";
	for (const auto &p : particles) {
		std::cout << "particles read = " << p << "\n";
	}
}

static GLuint dummy_vao = 0;
static GLuint shader = 0;
static std::unique_ptr<ParticleDataset> dataset = nullptr;
// Stupid thing we need for visus to find some non-existant config it shouldn't
// require b/c I'm using it as a standalone library not an app framework.
static Visus::UniquePtr<Visus::Application> visus_app = nullptr;

static void ui_fn(){
	if (!dataset) {
		return;
	}
	if (ImGui::Begin("IDX Particles")) {
		ImGui::Text("# of Particles %llu", dataset->particles.size());
	}
	ImGui::End();
}
static void render_fn(std::shared_ptr<glt::BufferAllocator> &allocator, const glm::mat4 &view,
		const glm::mat4 &proj, const float elapsed)
{
	if (dummy_vao == 0) {
		glGenVertexArrays(1, &dummy_vao);
	}
	if (shader == 0) {
		const std::string resource_path = glt::get_resource_path();
		shader = glt::load_program({std::make_pair(GL_VERTEX_SHADER, resource_path + "particle_idx_vert.glsl"),
				std::make_pair(GL_FRAGMENT_SHADER, resource_path + "particle_idx_frag.glsl")});
	}
	if (!dataset) {
		return;
	}
	glBindVertexArray(dummy_vao);
	glUseProgram(shader);

	if (dataset->vbo.buffer == 0) {
		// Just sending positions for now
		dataset->vbo = allocator->alloc(dataset->particles.size() * sizeof(glm::vec3));
		glm::vec3 *pos = static_cast<glm::vec3*>(dataset->vbo.map(GL_ARRAY_BUFFER, GL_MAP_WRITE_BIT));
		for (size_t i = 0; i < dataset->particles.size(); ++i) {
			pos[i].x = dataset->particles[i].x;
			pos[i].y = dataset->particles[i].y;
			pos[i].z = dataset->particles[i].z;
		}
		dataset->vbo.unmap(GL_ARRAY_BUFFER);

		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)dataset->vbo.offset);
	}

	glEnable(GL_PROGRAM_POINT_SIZE);
	glDrawArrays(GL_POINTS, 0, dataset->particles.size());

	glUseProgram(0);
	glBindVertexArray(0);
}
static bool loader_fn(const vl::FileName &file_name){
	std::cout << "particle_idx plugin loading file: " << file_name << "\n";
	dataset = std::make_unique<ParticleDataset>(file_name);
	return true;
}
static bool vislight_plugin_particle_idx_init(const std::vector<std::string> &args,
		vl::PluginFunctionTable &fcns, vl::MessageDispatcher &dispatcher)
{
	// Setup required junk
	visus_app = Visus::UniquePtr<Visus::Application>(new Visus::Application());
	const char* visus_junk_cmdline[] = {args[0].c_str()};
	visus_app->init(0, nullptr);
	Visus::IdxModule::attach();

	std::cout << "particle_idx plugin loaded, args = {\n";
	for (const auto &a : args){
		std::cout << "\t" << a << "\n";
		vl::FileName fname = a;
		if (fname.extension() == "idx") {
			loader_fn(fname);
		}
	}
	std::cout << "}\n";

	fcns.plugin_type = vl::PluginType::UI_ELEMENT | vl::PluginType::RENDER | vl::PluginType::LOADER;
	fcns.ui_fn = ui_fn;
	fcns.render_fn = render_fn;
	fcns.loader_fn = loader_fn;
	fcns.file_extensions = "idx";
	return true;
}
static void unload_fn(){}

// Setup externally callable loaders so the plugin loader can find our stuff
PLUGIN_LOAD_FN(particle_idx)
PLUGIN_UNLOAD_FN(particle_idx, unload_fn)

