#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <random>

GLuint picnic_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > picnic_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("picnic.pnct"));
	picnic_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Load< Scene > picnic_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("picnic.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = picnic_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();
		drawable.pipeline = lit_color_texture_program_pipeline;
		drawable.pipeline.vao = picnic_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;
	});
});

PlayMode::PlayMode() : scene(*picnic_scene) {
    hotdogs.clear();
    falling_hotdogs.clear();
    dying_hotdogs.clear();
    apples.clear();

    // saving type, start, count to duplicate objects in game inspired by:
    // Alyssa Lee: https://github.com/lassyla/game2
    for (auto &drawable : scene.drawables) {
		if (drawable.transform->name == "Bottle") {
			bottle = drawable.transform;
		} else if (drawable.transform->name == "Cursor") {
            cursor.cursor_transform = drawable.transform;
            glm::vec4 cursor_pos = glm::vec4((cursor.cursor_transform)->position.x, (cursor.cursor_transform)->position.y, (cursor.cursor_transform)->position.z, 1.0f);
            cursor.dir = glm::normalize(glm::vec3((cursor.cursor_transform)->make_local_to_world() * cursor_pos));
        } else if (drawable.transform->name == "Ketchup") {
            cursor.shot_transform = drawable.transform;
            cursor.shot_orig_rot = drawable.transform->rotation;
        } else if (drawable.transform->name == "Hotdog") {
            hotdog_init_transform = drawable.transform;
            hotdog_init_transform->position = offscreen_pos;
            hotdog_vertex_type = drawable.pipeline.type; 
			hotdog_vertex_start = drawable.pipeline.start; 
			hotdog_vertex_count = drawable.pipeline.count;
        } else if (drawable.transform->name == "Hit") {
            cursor.hit_transform = drawable.transform;
            cursor.hit_transform->position = offscreen_pos;
        } else if (drawable.transform->name == "Plate") {
            plate_init_transform = drawable.transform;
            plate_init_transform->position = offscreen_pos;
            plate_vertex_type = drawable.pipeline.type; 
			plate_vertex_start = drawable.pipeline.start; 
			plate_vertex_count = drawable.pipeline.count;
        } else if (drawable.transform->name == "Apple") {
            apple_init_transform = drawable.transform;
            apple_init_transform->position = offscreen_pos;
            apple_vertex_type = drawable.pipeline.type; 
			apple_vertex_start = drawable.pipeline.start; 
			apple_vertex_count = drawable.pipeline.count;
        }
	}

	//get pointer to camera for convenience:
	if (scene.cameras.size() != 1) throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();
}

PlayMode::~PlayMode() {
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {
	if (evt.type == SDL_KEYDOWN) {
		if (evt.key.keysym.sym == SDLK_ESCAPE) {
			SDL_SetRelativeMouseMode(SDL_FALSE);
			return true;
		} else if (evt.key.keysym.sym == SDLK_SPACE) {
            if (health <= 0) return false;
            if (cursor.shooting) return true;
            cursor.shooting = true;
            glm::vec4 cursor_pos = glm::vec4((cursor.cursor_transform)->position.x, (cursor.cursor_transform)->position.y, (cursor.cursor_transform)->position.z, 1.0f);
            cursor.dir = glm::normalize(glm::vec3((cursor.cursor_transform)->make_local_to_world() * cursor_pos));
            cursor.shot_transform->rotation = bottle->rotation * cursor.shot_orig_rot;
            return true;
        }
    } else if (evt.type == SDL_MOUSEBUTTONDOWN) {
		if (SDL_GetRelativeMouseMode() == SDL_FALSE) {
			SDL_SetRelativeMouseMode(SDL_TRUE);
			return true;
		}
	} else if (evt.type == SDL_MOUSEMOTION) {
        if (health <= 0) return false;
		if (SDL_GetRelativeMouseMode() == SDL_TRUE) {
			glm::vec2 motion = glm::vec2(
				evt.motion.xrel / float(window_size.x),
				-evt.motion.yrel / float(window_size.y)
			);
			bottle->rotation = glm::normalize(
				bottle->rotation
				* glm::angleAxis(motion.x * camera->fovy, glm::vec3(0.0f, 1.0f, 0.0f))
				* glm::angleAxis(motion.y * camera->fovy, glm::vec3(1.0f, 0.0f, 0.0f))
			);
			return true;
		}
	}

	return false;
}

void PlayMode::update(float elapsed) {
    // is_between is taken from https://stackoverflow.com/questions/328107/how-can-you-determine-a-point-is-between-two-other-points-on-a-line-segment
    auto is_between = [](glm::vec2 &a, glm::vec2 &b, glm::vec2 &c) { // c is point in middle
        float crossproduct = (c.y - a.y) * (b.x - a.x) - (c.x - a.x) * (b.y - a.y);
        float epsilon = 0.01f;
        if (std::abs(crossproduct) > epsilon) return false;

        float dotproduct = (c.x - a.x) * (b.x - a.x) + (c.y - a.y)*(b.y - a.y);
        if (dotproduct < 0.f) return false;

        float squaredlengthba = (b.x - a.x)*(b.x - a.x) + (b.y - a.y)*(b.y - a.y);
        if (dotproduct > squaredlengthba) return false;

        return true;
	};

    auto create_and_add_hotdog = [this]() {
        Hotdog hotdog;
        hotdog.num = hotdog_counter;
        hotdog.transform = new Scene::Transform;
        hotdog.transform->rotation = hotdog_init_transform->rotation;
        hotdog.transform->scale = hotdog_init_transform->scale;
        hotdog.transform->name = "Hotdog_" + std::to_string(hotdog_counter);

        hotdog.plate_transform = new Scene::Transform;
        hotdog.plate_transform->parent = hotdog.transform;
        hotdog.plate_transform->rotation = plate_init_transform->rotation;
        hotdog.plate_transform->scale = glm::vec3(0.f);
        hotdog.plate_transform->name = "Plate_" + std::to_string(hotdog_counter);
        
        hotdog_counter++;

        // generate hotdog movement
        float x = -5.f + static_cast <float> (rand()) /( static_cast <float> (RAND_MAX/10.f));
        hotdog.transform->position = glm::vec3(x, 9.f, 0.1f);
        hotdog.points[0] = hotdog.transform->position;
        for (int i=1; i < 3; ++i) {
            float x = -5.f + static_cast <float> (rand()) /( static_cast <float> (RAND_MAX/10.f));
            float y = static_cast <float> (rand()) /( static_cast <float> (RAND_MAX/hotdog.points[i-1].y));
            hotdog.points[i] = glm::vec3(x, y, 0.1f);
        }
        hotdog.points[2].y = 0.f;
        hotdogs.push_back(hotdog);

        // add hotdog to scene
        // using saved attributes to duplicate objects in game inspired by:
        // Alyssa Lee: https://github.com/lassyla/game2
        scene.drawables.emplace_back(hotdog.transform);
        Scene::Drawable &hotdog_drawable = scene.drawables.back();
        hotdog_drawable.pipeline = lit_color_texture_program_pipeline;
        hotdog_drawable.pipeline.vao = picnic_meshes_for_lit_color_texture_program;
        hotdog_drawable.pipeline.type = hotdog_vertex_type;
        hotdog_drawable.pipeline.start = hotdog_vertex_start;
        hotdog_drawable.pipeline.count = hotdog_vertex_count;

        scene.drawables.emplace_back(hotdog.plate_transform);
        Scene::Drawable &plate_drawable = scene.drawables.back();
        plate_drawable.pipeline = lit_color_texture_program_pipeline;
        plate_drawable.pipeline.vao = picnic_meshes_for_lit_color_texture_program;
        plate_drawable.pipeline.type = plate_vertex_type;
        plate_drawable.pipeline.start = plate_vertex_start;
        plate_drawable.pipeline.count = plate_vertex_count;
    };

    auto create_and_add_apple = [this]() {
        Apple apple;
        apple.num = apple_counter;
        apple.transform = new Scene::Transform;
        apple.transform->rotation = apple_init_transform->rotation;
        apple.transform->scale = apple_init_transform->scale;
        apple.transform->name = "Apple_" + std::to_string(apple_counter);
        apple_counter++;

        float y = static_cast <float> (rand()) /( static_cast <float> (RAND_MAX/10.f));
        apple.init_pos.y = y;

        float dv = -2.f + static_cast <float> (rand()) /( static_cast <float> (RAND_MAX/4.f));
        apple.init_vel.x += dv;
        apple.transform->position = apple.init_pos;

        apples.push_back(apple);

        // add apple to scene
        scene.drawables.emplace_back(apple.transform);
        Scene::Drawable &apple_drawable = scene.drawables.back();
        apple_drawable.pipeline = lit_color_texture_program_pipeline;
        apple_drawable.pipeline.vao = picnic_meshes_for_lit_color_texture_program;
        apple_drawable.pipeline.type = apple_vertex_type;
        apple_drawable.pipeline.start = apple_vertex_start;
        apple_drawable.pipeline.count = apple_vertex_count;
    };

    // spawn new hotdogs
    hotdog_spawn_time += elapsed;
    if (hotdog_spawn_time >= hotdog_spawn_timer && hotdogs.size() < 10) {
        hotdog_spawn_time = 0.f;
        create_and_add_hotdog();
    }

    // spawn new apples
    apple_spawn_time += elapsed;
    if (apple_spawn_time >= apple_spawn_timer && apples.size() < 3) {
        apple_spawn_time = 0.f;
        create_and_add_apple();
    }


    // animate shooting
    if (cursor.shooting) {
        cursor.shot_time += elapsed;
        if (cursor.shot_time < cursor.shot_expire_time) {
            cursor.shot_transform->position += cursor.dir * elapsed * cursor.shot_speed;
        } else {
            cursor.shooting = false;
            cursor.shot_transform->position = cursor.non_shot_pos;
            cursor.shot_time = 0.f;
        }
    }

    auto remove_hotdog_from_scene = [this](int num) {
        int found = 0;
        for (auto it = scene.drawables.begin(); it != scene.drawables.end(); it++) {
            auto &hotdog_drawable = (*it);
            if (hotdog_drawable.transform->name == "Hotdog_" + std::to_string(num)) {
                scene.drawables.erase(it--);
                found++;
            } else if (hotdog_drawable.transform->name == "Plate_" + std::to_string(num)) {
                scene.drawables.erase(it--);
                found++;
            }

            if (found == 2) {
                return;
            }
        }
    };

    // move hotdogs
    for (auto it = hotdogs.begin(); it != hotdogs.end(); it++) {
        auto &hotdog = (*it);
        glm::vec2 a = glm::vec2(hotdog.points[hotdog.current_idx-1].x, hotdog.points[hotdog.current_idx-1].y);
        glm::vec2 b = glm::vec2(hotdog.points[hotdog.current_idx].x, hotdog.points[hotdog.current_idx].y);
        glm::vec2 hotdog_v = b - a;
        glm::vec2 new_pos = glm::vec2(hotdog.transform->position.x, hotdog.transform->position.y) + (glm::normalize(hotdog_v) * hotdog.speed * elapsed);
        if (!is_between(a, b, new_pos)) { // reached point
            hotdog.transform->position = hotdog.points[hotdog.current_idx];
            hotdog.current_idx++;
            if (hotdog.current_idx >= 3) {
                falling_hotdogs.push_back(hotdog);
                hotdogs.erase(it--);
            }
        } else {
            hotdog.transform->position = glm::vec3(new_pos, 0.1f);
        }
    }

    auto remove_apple_from_scene = [this](int num) {
        for (auto it = scene.drawables.begin(); it != scene.drawables.end(); it++) {
            auto &apple_drawable = (*it);
            if (apple_drawable.transform->name == "Apple_" + std::to_string(num)) {
                scene.drawables.erase(it--);
                return;
            }
        }
    };

    // move apples
     for (auto it = apples.begin(); it != apples.end(); it++) {
        auto &apple = (*it);
        apple.time += elapsed;
        if (apple.time < apple.time_out) {
            if (apple.hit) {
                apple.transform->rotation *= glm::angleAxis(-9.0f * elapsed, glm::vec3(1, 0, 0));
                apple.transform->position += glm::vec3(0.0f, 10.0f * elapsed, 0.0f);
            } else {
                float apple_x = apple.init_pos.x + apple.init_vel.x * apple.time;
                float gravity = -1.f;
                float apple_z = apple.init_pos.z + apple.init_vel.z * apple.time + (gravity/2.f) * apple.time * apple.time;
                apple.transform->position = glm::vec3(apple_x, apple.init_pos.y, apple_z);
                apple.transform->rotation *= glm::angleAxis(2.0f * elapsed, glm::vec3(0, 1, 0));
            }
        } else { // apple has expired
            remove_apple_from_scene(apple.num);
            apples.erase(it--);
        }
    }

    auto point_hotdog_collision = [this](Hotdog &hotdog) {
        glm::vec3 shot_pos = cursor.shot_transform->position;
        glm::vec3 hotdog_pos = hotdog.transform->position;
        glm::vec3 hotdog_min = hotdog_pos - hotdog.radius;
        glm::vec3 hotdog_max = hotdog_pos + hotdog.radius;
        return (   shot_pos.x >= hotdog_min.x && shot_pos.x <= hotdog_max.x
            &&     shot_pos.y >= hotdog_min.y && shot_pos.y <= hotdog_max.y
            &&     shot_pos.z >= hotdog_min.z && shot_pos.z <= hotdog_max.z);
    };

    // check ketchup hotdog collision
    for (auto it = hotdogs.begin(); it != hotdogs.end(); it++) {
        auto &hotdog = (*it);
        if (point_hotdog_collision(hotdog)) {
            cursor.hit_transform->position = cursor.shot_transform->position;
            cursor.hit_transform->position.y -= 0.1f;

            cursor.shooting = false; //reset shot
            cursor.shot_transform->position = cursor.non_shot_pos;
            cursor.shot_time = 0.f;

            hotdog.hit = true;
            dying_hotdogs.push_back(hotdog);
            hotdogs.erase(it--);
        }
    }

    auto point_apple_collision = [this](Apple &apple) {
        glm::vec3 shot_pos = cursor.shot_transform->position;
        glm::vec3 apple_pos = apple.transform->position;
        glm::vec3 apple_min = apple_pos - apple.radius;
        glm::vec3 apple_max = apple_pos + apple.radius;
        return (   shot_pos.x >= apple_min.x && shot_pos.x <= apple_max.x
            &&     shot_pos.y >= apple_min.y && shot_pos.y <= apple_max.y
            &&     shot_pos.z >= apple_min.z && shot_pos.z <= apple_max.z);
    };

    // check ketchup apple collision
    for (auto it = apples.begin(); it != apples.end(); it++) {
        auto &apple = (*it);
        if (point_apple_collision(apple) && !apple.hit) {
            health -= 3;
            if (health < 0) health = 0;
            cursor.shooting = false; //reset shot
            cursor.shot_transform->position = cursor.non_shot_pos;
            cursor.shot_time = 0.f;

            apple.hit = true;
        }
    }

    // make hotdogs fall off the edge
    for (auto it = falling_hotdogs.begin(); it != falling_hotdogs.end(); it++) {
        auto &hotdog = (*it);
        hotdog.fall_time += elapsed;
        if (hotdog.fall_time < hotdog.fall_expire_timer) {
            glm::vec3 falling_transfrom = glm::vec3(0.f, 0.f, -0.3f);
            hotdog.transform->position += falling_transfrom;
        } else {
            health -= 1;
            if (health < 0) health = 0;
            remove_hotdog_from_scene(hotdog.num);
            falling_hotdogs.erase(it--);
        }
    }

    // animate hotdog death
    for (auto it = dying_hotdogs.begin(); it != dying_hotdogs.end(); it++) {
        auto &hotdog = (*it);
        hotdog.death_time += elapsed;
        if (hotdog.death_time < hotdog.death_standstill_timer) { // freeze animation
            continue;
        } else if (hotdog.death_time < hotdog.death_fall_timer) { // falling animation
            hotdog.plate_transform->scale = plate_init_transform->scale;
            hotdog.transform->rotation *= glm::angleAxis(-3.0f * elapsed, glm::vec3(1, 0, 0));
            hotdog.transform->position += glm::vec3(0.f, 0.f, -0.8f * elapsed);
        } else {
            remove_hotdog_from_scene(hotdog.num);
            dying_hotdogs.erase(it--);
        }
    }
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	//update camera aspect ratio for drawable:
	camera->aspect = float(drawable_size.x) / float(drawable_size.y);

	//set up light type and position for lit_color_texture_program:
	// TODO: consider using the Light(s) in the scene to do this
	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,-1.0f)));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
	glUseProgram(0);

	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	GL_ERRORS(); //print any errors produced by this setup code

	scene.draw(*camera);

	{ //use DrawLines to overlay some text:
		glDisable(GL_DEPTH_TEST);
		float aspect = float(drawable_size.x) / float(drawable_size.y);
		DrawLines lines(glm::mat4(
			1.0f / aspect, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		));

		constexpr float H = 0.09f;
        std::string lose = "";
        if (health <= 0) {
            lose = "       YOU LOSE!!!!";
        }

		lines.draw_text("                               Health: " + std::to_string(health) + lose,
			glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		float ofs = 2.0f / drawable_size.y;
		lines.draw_text("                               Health: " + std::to_string(health) + lose,
			glm::vec3(-aspect + 0.1f * H + ofs, -1.0 + + 0.1f * H + ofs, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));
	}
}
