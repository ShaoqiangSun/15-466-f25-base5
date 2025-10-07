#include "Mode.hpp"

#include "Connection.hpp"
#include "Game.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>
#include "Scene.hpp"
#include "GlyphCache.hpp"

struct PlayMode : Mode {
	PlayMode(Client &client);
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//----- game state -----

	//input tracking for local player:
	Player::Controls controls;

	//latest game state (from server):
	Game game;

	//last message from server:
	std::string server_message;

	//connection to server:
	Client &client;

	//camera:
	Scene::Camera *camera = nullptr;
	//local copy of the game scene (so code can change it during gameplay):
	Scene scene;

	Scene dynamic_scene;

	FT_Library ft_lib = nullptr;
  	FT_Face    ft_face = nullptr;
	hb_font_t* hb_font = nullptr;
	hb_buffer_t* hb_buf = nullptr;
	int pixel_size = 48;

	GlyphCache glyph_cache = GlyphCache(1024, 1024, 64, 64);
	struct Vertex {float x, y, u, v;};

	 
	void draw_text(const std::string& text, float origin_x, float baseline_y, float drawable_x, float drawable_y, const glm::vec4& color);

	std::list<Scene::Drawable>::iterator create_drawable(Scene &scene, std::list<Scene::Drawable>::iterator it_src);
	void delete_drawable(Scene &scene, std::list<Scene::Drawable>::iterator it);

	std::unordered_map<uint8_t, std::list<Scene::Drawable>::iterator> player_to_drawable;
};
