#include "ObserveMode.hpp"
#include "DrawLines.hpp"
#include "LitColorTextureProgram.hpp"
#include "Mesh.hpp"
#include "Sprite.hpp"
#include "DrawSprites.hpp"
#include "data_path.hpp"
#include "Sound.hpp"

#include <iostream>

Load< Sound::Sample > noise(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("cold-dunes.opus"));
});

Load< SpriteAtlas > trade_font_atlas(LoadTagDefault, []() -> SpriteAtlas const * {
	return new SpriteAtlas(data_path("trade-font"));
});

GLuint meshes_for_lit_color_texture_program = 0;
static Load< MeshBuffer > meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer *ret = new MeshBuffer(data_path("city.pnct"));
	meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

static Load< Scene > scene(LoadTagLate, []() -> Scene const * {
	Scene *ret = new Scene();
	ret->load(data_path("city.scene"), [](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		auto &mesh = meshes->lookup(mesh_name);
		scene.drawables.emplace_back(transform);
		Scene::Drawable::Pipeline &pipeline = scene.drawables.back().pipeline;

		pipeline = lit_color_texture_program_pipeline;
		pipeline.vao = meshes_for_lit_color_texture_program;
		pipeline.type = mesh.type;
		pipeline.start = mesh.start;
		pipeline.count = mesh.count;
	});
	return ret;
});

ObserveMode::ObserveMode() {
	assert(scene->cameras.size() && "Observe requires cameras.");

	current_camera = &scene->cameras.front();

	// Find birds
	for (auto it = scene->transforms.begin(); it != scene->transforms.end(); it++) {
		if (it->name.substr(0, 4) == "BIRD") {
			birds.push_back(&(*it));
		} 
	}
	std::cout << "Found " << birds.size() << " birds." << std::endl;

	noise_loop = Sound::loop_3D(*noise, 1.0f, glm::vec3(0.0f, 0.0f, 0.0f), 10.0f);
}

ObserveMode::~ObserveMode() {
	noise_loop->stop();
}

bool ObserveMode::check_ray_bird(glm::vec3 ray, const Scene::Transform* bird) {
	glm::vec3 O = current_camera->transform->position;
	glm::vec3 C = bird->position;
	float r = 0.1f;

	float b = glm::dot(ray, O - C);
	float c = glm::dot(O - C, O - C) - r * r;

	return (b * b - c >= 0);
}

void ObserveMode::raycast_click(int x, int y) {
	// Fix the screen dimensions
	GLint dim_viewport[4];
	glGetIntegerv(GL_VIEWPORT, dim_viewport);
	int width = dim_viewport[2];
	int height = dim_viewport[3];
	
	// Convert from viewport coords to normalized device coords
	glm::vec3 ray_nds = glm::vec3(2.0f * x / width - 1.0f, 1.0f - (2.0f * y) / height, 1.0f);

	// Convert from nds to homogenous clip coords
	glm::vec4 ray_clip = glm::vec4(ray_nds.x, ray_nds.y, -1.0, 1.0);

	// Convert from clip coords to camera coords
	glm::vec4 ray_cam = glm::inverse(current_camera->make_projection()) * ray_clip;
	ray_cam = glm::vec4(ray_cam.x, ray_cam.y, -1.0, 0.0);

	// Convert from camera coords to world
	glm::vec4 ray_wort = glm::inverse(current_camera->transform->make_world_to_local()) * ray_cam;
	glm::vec3 ray_wor = glm::vec3(ray_wort.x, ray_wort.y, ray_wort.z);
	ray_wor = glm::normalize(ray_wor);

	// Check the ray against all birds
	for (auto it = birds.begin(); it != birds.end(); it++) {
		if (check_ray_bird(ray_wor, *it)) {
			found_birds.push_back(*it);
		}
	}
} 

bool ObserveMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {
	if (evt.type == SDL_KEYDOWN) {
		if (evt.key.keysym.sym == SDLK_LEFT) {
			auto ci = scene->cameras.begin();
			while (ci != scene->cameras.end() && &*ci != current_camera) ++ci;
			if (ci == scene->cameras.begin()) ci = scene->cameras.end();
			--ci;
			current_camera = &*ci;
			return true;
		} else if (evt.key.keysym.sym == SDLK_RIGHT) {
			auto ci = scene->cameras.begin();
			while (ci != scene->cameras.end() && &*ci != current_camera) ++ci;
			if (ci != scene->cameras.end()) ++ci;
			if (ci == scene->cameras.end()) ci = scene->cameras.begin();
			current_camera = &*ci;

			return true;
		}
	} else if (evt.type == SDL_MOUSEBUTTONDOWN) {
		if (evt.button.button == SDL_BUTTON_LEFT) {
			raycast_click(evt.button.x, evt.button.y);
		}
	}
	
	return false;
}

void ObserveMode::update(float elapsed) {
	noise_angle = std::fmod(noise_angle + elapsed, 2.0f * 3.1415926f);

	//update sound position:
	glm::vec3 center = glm::vec3(10.0f, 4.0f, 1.0f);
	float radius = 10.0f;
	noise_loop->set_position(center + radius * glm::vec3( std::cos(noise_angle), std::sin(noise_angle), 0.0f));

	//update listener position:
	glm::mat4 frame = current_camera->transform->make_local_to_world();

	//using the sound lock here because I want to update position and right-direction *simultaneously* for the audio code:
	Sound::lock();
	Sound::listener.set_position(frame[3]);
	Sound::listener.set_right(frame[0]);
	Sound::unlock();

	// Update found birds
	for (auto it = found_birds.begin(); it != found_birds.end(); it++) {
		const_cast<Scene::Transform*>(*it)->position += glm::vec3(0.0f, 0.0f, 0.1f);
	}
}

void ObserveMode::draw(glm::uvec2 const &drawable_size) {
	//--- actual drawing ---
	glClearColor(0.85f, 0.85f, 0.90f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

	const_cast< Scene::Camera * >(current_camera)->aspect = drawable_size.x / float(drawable_size.y);

	scene->draw(*current_camera);

	{ //help text overlay:
		glDisable(GL_DEPTH_TEST);
		glEnable(GL_BLEND);
		glBlendEquation(GL_FUNC_ADD);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		DrawSprites draw(*trade_font_atlas, glm::vec2(0,0), glm::vec2(320, 200), drawable_size, DrawSprites::AlignPixelPerfect);

		std::string bird_text = "FOUND " + std::to_string(found_birds.size()) + "/" + std::to_string(birds.size()) + " BIRDS";
		std::string help_text = "--- SWITCH CAMERAS WITH LEFT/RIGHT ---";
		glm::vec2 min, max;
		draw.get_text_extents(help_text, glm::vec2(0.0f, 0.0f), 1.0f, &min, &max);
		float x = std::round(160.0f - (0.5f * (max.x + min.x)));
		draw.draw_text(help_text, glm::vec2(x, 1.0f), 1.0f, glm::u8vec4(0x00,0x00,0x00,0xff));
		draw.draw_text(help_text, glm::vec2(x, 2.0f), 1.0f, glm::u8vec4(0xff,0xff,0xff,0xff));

		draw.get_text_extents(bird_text, glm::vec2(0.0f, 0.0f), 1.0f, &min, &max);
		x = std::round(160.0f - (0.5f * (max.x + min.x)));
		draw.draw_text(bird_text, glm::vec2(x, -10.0f), 1.0f, glm::u8vec4(0x00,0x00,0x00,0xff));
		draw.draw_text(bird_text, glm::vec2(x, -12.0f), 1.0f, glm::u8vec4(0xff,0xff,0xff,0xff));
	}
}
