#include "Mode.hpp"

#include "Scene.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>

struct PlayMode : Mode {
	PlayMode();
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//----- game state -----

    // for duplicating hotdogs
    GLenum hotdog_vertex_type = GL_TRIANGLES; 
	GLuint hotdog_vertex_start = 0; 
	GLuint hotdog_vertex_count = 0;
    Scene::Transform *hotdog_init_transform;

    // for duplicating plates
    GLenum plate_vertex_type = GL_TRIANGLES; 
	GLuint plate_vertex_start = 0; 
	GLuint plate_vertex_count = 0;
    Scene::Transform *plate_init_transform;

    // for duplicating apples
    GLenum apple_vertex_type = GL_TRIANGLES; 
	GLuint apple_vertex_start = 0; 
	GLuint apple_vertex_count = 0;
    Scene::Transform *apple_init_transform;

    int health = 20;

    struct Cursor {
        // for aiming
        Scene::Transform *cursor_transform;
        glm::vec3 dir;

        // animating shot
        Scene::Transform *shot_transform;

        // animate hit
        Scene::Transform *hit_transform;

        bool shooting = false;
        float shot_time = 0.f;
        float shot_expire_time = 1.5f; // make shot disappear after certain amount of time
        float shot_speed = 20.0f;
        glm::quat shot_orig_rot;

        bool hit;
        float hit_time;
        glm::vec3 non_shot_pos = glm::vec3(0.f);
    } cursor;

    glm::vec3 offscreen_pos = glm::vec3(-20.f, 0.f, 0.f);

    struct Hotdog {
        Scene::Transform *transform;
        int current_idx = 1;
        glm::vec3 points[3]; // movement points
        float speed = 1.5f;
        int num;
        // glm::vec3 radius = glm::vec3(0.1f, 0.16f, 0.325f); // actual radius
        glm::vec3 radius = glm::vec3(0.15f, 0.45f, 0.65f); // increase radius to account for ketchup speed
        
        //death animation
        float death_time = 0.f;
        float death_standstill_timer = 0.5f;
        float death_fall_timer = 1.0f;
        bool hit = false;
        Scene::Transform *plate_transform;

        //falling animation
        float fall_expire_timer = 1.0f;
        float fall_time = 0.f;
    };

    int hotdog_counter = 0;

    std::vector< Hotdog > hotdogs;
    float hotdog_spawn_time = 3.f;
    float hotdog_spawn_timer = 2.f;

    std::vector< Hotdog > falling_hotdogs;
    std::vector< Hotdog > dying_hotdogs;

    struct Apple {
        Scene::Transform *transform;
        glm::vec3 init_pos = glm::vec3(-6, 5, 0); // vary y
        glm::vec3 init_vel = glm::vec3(8, 0, 1.f);
        float time = 0.f;
        float time_out = 3.f;
        int num;
        bool hit = false;

        // glm::vec3 radius = glm::vec3(0.3f, 0.3f, 0.3f); // actual radius
        glm::vec3 radius = glm::vec3(0.35f, 0.35f, 0.35f); // increase radius to account for ketchup speed
    };

    int apple_counter = 0;

    std::vector< Apple > apples;
    float apple_spawn_time = 0.f;
    float apple_spawn_timer = 1.f;

	//local copy of the game scene (so code can change it during gameplay):
	Scene scene;

    //transforms
    Scene::Transform *bottle = nullptr;
	
	//camera:
	Scene::Camera *camera = nullptr;

};
