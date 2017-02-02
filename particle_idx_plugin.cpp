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
	glt::SubBuffer vbo, boundary_vbo;
	glm::mat4 logic_to_physic;
	glm::vec3 logic_box_min, logic_box_max;
	glm::vec3 query_box_min, query_box_max;
	bool query_box_changed, particles_changed;
	int resolution;

	ParticleDataset(const vl::FileName &fname);
	// Query the currently held query box from the IDX file
	void query_box();
};
ParticleDataset::ParticleDataset(const vl::FileName &fname)
	: dataset(Visus::Dataset::loadDataset(fname.file_name)), logic_to_physic(glm::mat4(1))
{
	using namespace Visus;
	if (!dataset || !dataset->valid()) {
		throw std::runtime_error("Failed to load IDX dataset");
	}
	const NdBox world_box = dataset->getLogicBox();
	std::cout << "World box = " << world_box.toString() << "\n"
		<< "world box p1 = " << world_box.p1().toString() << "\n"
		<< "world box p2 = " << world_box.p2().toString() << "\n";
	const Matrix lp = dataset->getLogicToPhysic();
	logic_to_physic[0][0] = lp(0, 0);
	// Note: glm is column major
	logic_to_physic[3][0] = lp(0, 3);
	logic_to_physic[1][1] = lp(1, 1);
	logic_to_physic[3][1] = lp(1, 3);
	logic_to_physic[2][2] = lp(2, 2);
	logic_to_physic[3][2] = lp(2, 3);

	// Box goes from a to b in logic space
	logic_box_min = glm::vec3(world_box.p1().x, world_box.p1().y, world_box.p1().z);
	logic_box_max = glm::vec3(world_box.p2().x, world_box.p2().y, world_box.p2().z);
	query_box_min = logic_box_min;
	query_box_max = logic_box_max;
	query_box_changed = true;
	particles_changed = false;
	std::cout << "max resolution = " << dataset->getMaxResolution() << std::endl;
	resolution = dataset->getMaxResolution();
	// Query the initial data we want to display
	query_box();
}
void ParticleDataset::query_box() {
	using namespace Visus;

	// Our "field" is just a bin holding 2 particles
	Field field("particle", DType(PARTICLES_PER_SAMPLE * sizeof(Particle), DTypes::UINT8));
	std::cout << "particle field bytes/sample = " << field.dtype.getByteSize(1) << "\n";
	field.default_layout="";

	auto access = dataset->createAccess();
	NdBox query_box;
	query_box.setP1(NdPoint(query_box_min.x, query_box_min.y, query_box_min.z, 0, 0));
	query_box.setP2(NdPoint(query_box_max.x, query_box_max.y, query_box_max.z, 1, 1));
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
	std::cout << "max resolution = " << dataset->getMaxResolution()
		<< ", query resolution = " << resolution << std::endl;
	query->addEndResolution(resolution);

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
	particles.clear();
	std::copy_if(p_data, p_data + possible_particles, std::back_inserter(particles),
			[](const Particle &p) { return p.valid != 0; });
	std::cout << "valid particles in the query = " << particles.size() << "\n";
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
		ImGui::Text("Logic Box [%.2f, %.2f, %.2f] to [%.2f, %.2f, %.2f]",
				dataset->logic_box_min.x, dataset->logic_box_min.y, dataset->logic_box_min.z,
				dataset->logic_box_max.x, dataset->logic_box_max.y, dataset->logic_box_max.z);
		// Is the max range inclusive or exclusive for the slider?
		if (ImGui::SliderInt("Resolution", &dataset->resolution, 0, dataset->dataset->getMaxResolution())) {
			dataset->particles_changed = true;
		}
		if (ImGui::InputFloat3("Box Min", &dataset->query_box_min.x, -1, ImGuiInputTextFlags_EnterReturnsTrue)) {
			dataset->query_box_changed = true;
			dataset->particles_changed = true;
		}
		if (ImGui::InputFloat3("Box Max", &dataset->query_box_max.x, -1, ImGuiInputTextFlags_EnterReturnsTrue)) {
			dataset->query_box_changed = true;
			dataset->particles_changed = true;
		}
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
	}
	// Setup buffer to draw the border of the query region
	if (dataset->boundary_vbo.buffer == 0) {
		// We have 12 lines w/ 2 points each
		dataset->boundary_vbo = allocator->alloc(24 * sizeof(glm::vec3));
	}
	if (dataset->query_box_changed || dataset->particles_changed) {
		dataset->query_box();
	}
	if (dataset->particles_changed) {
		dataset->particles_changed = false;
		// Upload the new queried position data
		allocator->free(dataset->vbo);
		dataset->vbo = allocator->alloc(dataset->particles.size() * sizeof(glm::vec3));
		glm::vec3 *pos = static_cast<glm::vec3*>(dataset->vbo.map(GL_ARRAY_BUFFER, GL_MAP_WRITE_BIT));
		for (size_t i = 0; i < dataset->particles.size(); ++i) {
			pos[i].x = dataset->particles[i].x;
			pos[i].y = dataset->particles[i].y;
			pos[i].z = dataset->particles[i].z;
		}
		dataset->vbo.unmap(GL_ARRAY_BUFFER);
	}
	if (dataset->query_box_changed) {
		dataset->query_box_changed = false;
		const glm::vec3 min = glm::vec3(dataset->logic_to_physic * glm::vec4(dataset->query_box_min, 1.0));
		const glm::vec3 max = glm::vec3(dataset->logic_to_physic * glm::vec4(dataset->query_box_max, 1.0));
		glm::vec3 *lines = static_cast<glm::vec3*>(dataset->boundary_vbo.map(GL_ARRAY_BUFFER, GL_MAP_WRITE_BIT));
		lines[0] = min;
		lines[1] = glm::vec3(max.x, min.y, min.z);

		lines[2] = min;
		lines[3] = glm::vec3(min.x, max.y, min.z);

		lines[4] = glm::vec3(max.x, min.y, min.z);
		lines[5] = glm::vec3(max.x, max.y, min.z);

		lines[6] = glm::vec3(min.x, max.y, min.z);
		lines[7] = glm::vec3(max.x, max.y, min.z);

		lines[8] = glm::vec3(min.x, min.y, max.z);
		lines[9] = glm::vec3(max.x, min.y, max.z);

		lines[10] = glm::vec3(min.x, min.y, max.z);
		lines[11] = glm::vec3(min.x, max.y, max.z);

		lines[12] = glm::vec3(max.x, min.y, max.z);
		lines[13] = glm::vec3(max.x, max.y, max.z);

		lines[14] = glm::vec3(min.x, max.y, max.z);
		lines[15] = glm::vec3(max.x, max.y, max.z);

		lines[16] = min;
		lines[17] = glm::vec3(min.x, min.y, max.z);

		lines[18] = glm::vec3(max.x, min.y, min.z);
		lines[19] = glm::vec3(max.x, min.y, max.z);

		lines[20] = glm::vec3(min.x, max.y, min.z);
		lines[21] = glm::vec3(min.x, max.y, max.z);

		lines[22] = glm::vec3(max.x, max.y, min.z);
		lines[23] = glm::vec3(max.x, max.y, max.z);

		dataset->boundary_vbo.unmap(GL_ARRAY_BUFFER);
	}

	glEnable(GL_PROGRAM_POINT_SIZE);
	glBindBuffer(GL_ARRAY_BUFFER, dataset->vbo.buffer);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)dataset->vbo.offset);
	glDrawArrays(GL_POINTS, 0, dataset->particles.size());

	glBindBuffer(GL_ARRAY_BUFFER, dataset->boundary_vbo.buffer);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)dataset->boundary_vbo.offset);
	glDrawArrays(GL_LINES, 0, 24);

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

