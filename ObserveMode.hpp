#pragma once

#include "Mode.hpp"
#include "Scene.hpp"
#include "Sound.hpp"

struct ObserveMode : Mode {
	ObserveMode();
	virtual ~ObserveMode();

	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	// Unique routines
	void raycast_click(int x, int y);
	bool check_ray_bird(glm::vec3 ray, const Scene::Transform* bird);

	Scene::Camera const *current_camera = nullptr;

	std::shared_ptr< Sound::PlayingSample > noise_loop;
	float noise_angle = 0.0f;

	// List of birds
	std::vector<const Scene::Transform*> birds;
	std::vector<const Scene::Transform*> found_birds;
};
