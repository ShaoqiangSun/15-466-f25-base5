#include "PlayMode.hpp"

#include "DrawLines.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"
#include "hex_dump.hpp"

#include <glm/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

#include <random>
#include <array>

#include "Mesh.hpp"
#include "Load.hpp"
#include "LitColorTextureProgram.hpp"
#include "ColorTextureProgram.hpp"

//reference: https://github.com/ShaoqiangSun/15-466-f25-base2
GLuint hexapod_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > game_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("hide-and-seek.pnct"));
	hexapod_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

//reference: https://github.com/ShaoqiangSun/15-466-f25-base2
Load< Scene > game_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("hide-and-seek.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = game_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = hexapod_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;

	});
});

GLuint vbo = 0; 
GLuint vao = 0;


//reference: https://github.com/ShaoqiangSun/15-466-f25-base4
void init_text_render() {
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
	
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);

	glEnableVertexAttribArray(color_texture_program->Position_vec4);
	glVertexAttribPointer(color_texture_program->Position_vec4, 2, GL_FLOAT, GL_FALSE, sizeof(PlayMode::Vertex), (void*)0);

	glEnableVertexAttribArray(color_texture_program->TexCoord_vec2);
	glVertexAttribPointer(color_texture_program->TexCoord_vec2, 2, GL_FLOAT, GL_FALSE, sizeof(PlayMode::Vertex), (void*)(2 * sizeof(float)));
	
	glDisableVertexAttribArray(color_texture_program->Color_vec4);

	glBindVertexArray(0);
}

static glm::mat4 pixel_to_clip(float w, float h) {
    glm::mat4 M(1.0f);
    M[0][0] = 2.0f / w;
    M[1][1] = 2.0f / h;
    M[2][2] = 1.0f;
    M[3]    = glm::vec4(-1.0f, -1.0f, 0.0f, 1.0f);
    return M;
}

//reference: https://github.com/ShaoqiangSun/15-466-f25-base4
void PlayMode::draw_text(const std::string& text, float origin_x, float baseline_y, float drawable_x, float drawable_y, const glm::vec4& color) 
{
	hb_buffer_reset(hb_buf);
	hb_buffer_add_utf8(hb_buf, text.c_str(), -1, 0, -1);
	hb_buffer_guess_segment_properties(hb_buf);

	hb_shape(hb_font, hb_buf, NULL, 0);

	unsigned int len = hb_buffer_get_length(hb_buf);
	hb_glyph_info_t *info = hb_buffer_get_glyph_infos(hb_buf, NULL);
	hb_glyph_position_t *pos = hb_buffer_get_glyph_positions(hb_buf, NULL);

	std::unordered_map<int, std::vector<PlayMode::Vertex>> bucket;

	float current_x = 0;
	float current_y = 0;

	float total_advance = 0.f;
	for (unsigned i = 0; i < len; i++) {
		total_advance += pos[i].x_advance / 64.f;
	}

	origin_x = (drawable_x - total_advance) * 0.5f;

	for (unsigned int i = 0; i < len; i++) {
		hb_codepoint_t gid = info[i].codepoint;
		const GlyphEntry& g_entry = glyph_cache.get(ft_face, gid);

		float x_pos = origin_x + current_x + pos[i].x_offset / 64. + g_entry.left;
		float y_pos = baseline_y - (current_y + pos[i].y_offset / 64. - g_entry.top);

		if (g_entry.w > 0 && g_entry.h > 0) {
			std::vector<PlayMode::Vertex> &verts = bucket[g_entry.texture_id];

			verts.push_back({x_pos, y_pos - g_entry.h, 	g_entry.u0, g_entry.v1});
			verts.push_back({x_pos, y_pos,     			g_entry.u0, g_entry.v0});
			verts.push_back({x_pos + g_entry.w, y_pos,  g_entry.u1, g_entry.v0});
			verts.push_back({x_pos, y_pos - g_entry.h, 	g_entry.u0, g_entry.v1});
			verts.push_back({x_pos + g_entry.w, y_pos,  g_entry.u1, g_entry.v0});
			verts.push_back({x_pos + g_entry.w, y_pos - g_entry.h,  g_entry.u1, g_entry.v1});
		}
		
		current_x += pos[i].x_advance / 64.;
		current_y += pos[i].y_advance / 64.;
	}

	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	
	glUseProgram(color_texture_program->program);

	glm::mat4 M = pixel_to_clip(drawable_x, drawable_y);
	glUniformMatrix4fv(color_texture_program->CLIP_FROM_OBJECT_mat4, 1, GL_FALSE, glm::value_ptr(M));
	glVertexAttrib4f(color_texture_program->Color_vec4, color.r, color.g, color.b, color.a);

	glActiveTexture(GL_TEXTURE0);
	glBindVertexArray(vao);

	//reference: https://docs.gl/
	for (auto & [texture_id, verts] : bucket) {
		if (verts.empty()) continue;
		
		glBindTexture(GL_TEXTURE_2D, glyph_cache.textures[texture_id].tex_index);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(PlayMode::Vertex), verts.data(), GL_STREAM_DRAW);
		glDrawArrays(GL_TRIANGLES, 0, (GLsizei)verts.size());
	}

	glBindVertexArray(0);
	glUseProgram(0);
}

PlayMode::PlayMode(Client &client_) : client(client_), scene(*game_scene) {
	

	dynamic_scene = scene;

	
	auto it = std::next(dynamic_scene.drawables.begin());
	while (it != dynamic_scene.drawables.end()) {
		auto next = std::next(it);
		delete_drawable(dynamic_scene, it);
		it = next;
	}


	camera = &dynamic_scene.cameras.front();

	//reference: https://github.com/harfbuzz/harfbuzz-tutorial/blob/master/hello-harfbuzz-freetype.c
	if (FT_Init_FreeType(&ft_lib)) std::cerr << "ft lib init failed";
	if (FT_New_Face(ft_lib, "dist/Helvetica.ttc", 0, &ft_face)) std::cerr << "ft face init failed";

	FT_Set_Pixel_Sizes(ft_face, 0, pixel_size);

	hb_font = hb_ft_font_create(ft_face, NULL);
	hb_buf = hb_buffer_create();

	init_text_render();
}

PlayMode::~PlayMode() {
	if (hb_buf)  hb_buffer_destroy(hb_buf);
	if (hb_font) hb_font_destroy(hb_font);
	if (ft_face) FT_Done_Face(ft_face);
	if (ft_lib)  FT_Done_FreeType(ft_lib);
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (evt.type == SDL_EVENT_KEY_DOWN) {
		if (evt.key.repeat) {
			//ignore repeats
		} else if (evt.key.key == SDLK_A) {
			controls.left.downs += 1;
			controls.left.pressed = true;
			return true;
		} else if (evt.key.key == SDLK_D) {
			controls.right.downs += 1;
			controls.right.pressed = true;
			return true;
		} else if (evt.key.key == SDLK_W) {
			controls.up.downs += 1;
			controls.up.pressed = true;
			return true;
		} else if (evt.key.key == SDLK_S) {
			controls.down.downs += 1;
			controls.down.pressed = true;
			return true;
		} else if (evt.key.key == SDLK_SPACE) {
			controls.jump.downs += 1;
			controls.jump.pressed = true;
			return true;
		}
	} else if (evt.type == SDL_EVENT_KEY_UP) {
		if (evt.key.key == SDLK_A) {
			controls.left.pressed = false;
			return true;
		} else if (evt.key.key == SDLK_D) {
			controls.right.pressed = false;
			return true;
		} else if (evt.key.key == SDLK_W) {
			controls.up.pressed = false;
			return true;
		} else if (evt.key.key == SDLK_S) {
			controls.down.pressed = false;
			return true;
		} else if (evt.key.key == SDLK_SPACE) {
			controls.jump.pressed = false;
			return true;
		}
	}

	return false;
}

void PlayMode::update(float elapsed) {

	//queue data for sending to server:
	controls.send_controls_message(&client.connection);

	//reset button press counters:
	controls.left.downs = 0;
	controls.right.downs = 0;
	controls.up.downs = 0;
	controls.down.downs = 0;
	controls.jump.downs = 0;

	//send/receive data:
	client.poll([this](Connection *c, Connection::Event event){
		if (event == Connection::OnOpen) {
			std::cout << "[" << c->socket << "] opened" << std::endl;
		} else if (event == Connection::OnClose) {
			std::cout << "[" << c->socket << "] closed (!)" << std::endl;
			throw std::runtime_error("Lost connection to server!");
		} else { assert(event == Connection::OnRecv);
			//std::cout << "[" << c->socket << "] recv'd data. Current buffer:\n" << hex_dump(c->recv_buffer); std::cout.flush(); //DEBUG
			bool handled_message;
			try {
				do {
					handled_message = false;
					if (game.recv_state_message(c)) handled_message = true;
				} while (handled_message);
			} catch (std::exception const &e) {
				std::cerr << "[" << c->socket << "] malformed message from server: " << e.what() << std::endl;
				//quit the game:
				throw e;
			}
		}
	}, 0.0);

	
	for (auto &p : game.players) {
		
		if (player_to_drawable.count(p.player_id) == 0) {
			std::list<Scene::Drawable>::iterator it_src;
			if (p.role == Player::Role::Seeker) {
				it_src = std::next(scene.drawables.begin(), 1);
			}
			else if (p.role == Player::Role::Hider){
				it_src = std::next(scene.drawables.begin(), 2);			
			}

			auto it_dst = create_drawable(dynamic_scene, it_src);
			player_to_drawable[p.player_id] = it_dst;
		}
		
		if (p.is_caught) {
			if (player_to_drawable.count(p.player_id) == 1) {
				delete_drawable(dynamic_scene, player_to_drawable[p.player_id]);
				player_to_drawable.erase(p.player_id);
			}
		}

	}

	for (auto &p : game.players) {
		if (p.is_caught) continue;
		
		player_to_drawable[p.player_id]->transform->position = glm::vec3(p.position.x, p.position.y, 0.05f);
	}

}

void PlayMode::draw(glm::uvec2 const &drawable_size) {

	bool is_seeker = game.players.front().role == Player::Role::Seeker;
	float cut_off_cos = 0.0f;

	//update camera aspect ratio for drawable:
	camera->aspect = float(drawable_size.x) / float(drawable_size.y);

	// Scene::Light light = scene.lights.front();
	Scene::Light light = dynamic_scene.lights.front();

	glm::mat4x3 M_WS = light.transform->make_world_from_local();
	glm::vec3 light_pos = M_WS[3];
	glm::vec3 light_dir = -glm::normalize(M_WS[2]);

	int light_type = 0;
	switch (light.type) {
		case Scene::Light::Point: light_type = 0; break;
		case Scene::Light::Hemisphere: light_type = 1; break;
		case Scene::Light::Spot: light_type = 2; break;
		case Scene::Light::Directional: light_type = 3; break;
	}

	//
	if (is_seeker) {
		light_type = 2;
		Player seeker = game.players.front();
		glm::vec3 seeker_pos = glm::vec3(seeker.position.x, seeker.position.y, 0.05f);
		
		light_pos = seeker_pos + glm::vec3(0.0f, 0.0f, 10.0f);
		light_dir = glm::normalize(seeker_pos - light_pos);

		cut_off_cos = std::cos(light.spot_fov);
	}


	
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glDepthFunc(GL_LESS);

	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, light_type);
	glUniform3fv(lit_color_texture_program->LIGHT_LOCATION_vec3, 1, glm::value_ptr(light_pos));
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(light_dir));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(light.energy));
	glUniform1f(lit_color_texture_program->LIGHT_CUTOFF_float, cut_off_cos);
	glUseProgram(0);

	dynamic_scene.draw(*camera);


	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);
	glDepthFunc(GL_EQUAL);
	
	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 2);
	glUniform3fv(lit_color_texture_program->LIGHT_LOCATION_vec3, 1, glm::value_ptr(game.spotlight.pos));
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(game.spotlight.dir));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(game.spotlight.energy));
	glUniform1f(lit_color_texture_program->LIGHT_CUTOFF_float, std::cos(game.spotlight.cutoff));
	glUseProgram(0);

	dynamic_scene.draw(*camera);

	glDisable(GL_BLEND);

	

	GL_ERRORS(); //print any errors produced by this setup code

	// static std::array< glm::vec2, 16 > const circle = [](){
	// 	std::array< glm::vec2, 16 > ret;
	// 	for (uint32_t a = 0; a < ret.size(); ++a) {
	// 		float ang = a / float(ret.size()) * 2.0f * float(M_PI);
	// 		ret[a] = glm::vec2(std::cos(ang), std::sin(ang));
	// 	}
	// 	return ret;
	// }();

	// glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
	// glClear(GL_COLOR_BUFFER_BIT);
	glDisable(GL_DEPTH_TEST);

	FT_Size_Metrics m = ft_face->size->metrics;
	float asc =  m.ascender  / 64.0f;
	float desc = -m.descender / 64.0f;
	float H = (float)drawable_size.y;

	float baseline_y = H * 0.5f + (asc - desc) * 0.5f;
	std::string text;
	glm::vec4 col(1,1,1,1);
	if (game.game_state == Game::GameState::BeforeStart) {
		Player player = game.players.front();

		if (!player.is_ready) text = "Press Space to be ready";
		else text = "You are ready. Please wait for others to be ready";
		draw_text(text, drawable_size.x/2, baseline_y, drawable_size.x, drawable_size.y, col);
	}
	if (game.game_state == Game::GameState::Playing) {
		int timer_int = (int)std::ceil(game.timer);
		draw_text(std::to_string(timer_int), drawable_size.x/2, H - asc - 50.0f, drawable_size.x, drawable_size.y, col);
	}
	if (game.game_state == Game::GameState::SeekerWin) {
		text = "Seeker Wins!"; 
		draw_text(text, drawable_size.x/2, baseline_y, drawable_size.x, drawable_size.y, col);
	}
	if (game.game_state == Game::GameState::HiderWin) {
		text = "Hider Wins!"; 
		draw_text(text, drawable_size.x/2, baseline_y, drawable_size.x, drawable_size.y, col);
	}

	
	//figure out view transform to center the arena:
	float aspect = float(drawable_size.x) / float(drawable_size.y);
	float scale = std::min(
		2.0f * aspect / (Game::ArenaMax.x - Game::ArenaMin.x + 2.0f * Game::PlayerRadius),
		2.0f / (Game::ArenaMax.y - Game::ArenaMin.y + 2.0f * Game::PlayerRadius)
	);
	glm::vec2 offset = -0.5f * (Game::ArenaMax + Game::ArenaMin);

	glm::mat4 world_to_clip = glm::mat4(
		scale / aspect, 0.0f, 0.0f, offset.x,
		0.0f, scale, 0.0f, offset.y,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	);

	// {
	// 	DrawLines lines(world_to_clip);

	// 	//helper:
	// 	auto draw_text = [&](glm::vec2 const &at, std::string const &text, float H) {
	// 		lines.draw_text(text,
	// 			glm::vec3(at.x, at.y, 0.0),
	// 			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
	// 			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
	// 		float ofs = (1.0f / scale) / drawable_size.y;
	// 		lines.draw_text(text,
	// 			glm::vec3(at.x + ofs, at.y + ofs, 0.0),
	// 			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
	// 			glm::u8vec4(0xff, 0xff, 0xff, 0x00));
	// 	};

	// 	lines.draw(glm::vec3(Game::ArenaMin.x, Game::ArenaMin.y, 0.0f), glm::vec3(Game::ArenaMax.x, Game::ArenaMin.y, 0.0f), glm::u8vec4(0xff, 0x00, 0xff, 0xff));
	// 	lines.draw(glm::vec3(Game::ArenaMin.x, Game::ArenaMax.y, 0.0f), glm::vec3(Game::ArenaMax.x, Game::ArenaMax.y, 0.0f), glm::u8vec4(0xff, 0x00, 0xff, 0xff));
	// 	lines.draw(glm::vec3(Game::ArenaMin.x, Game::ArenaMin.y, 0.0f), glm::vec3(Game::ArenaMin.x, Game::ArenaMax.y, 0.0f), glm::u8vec4(0xff, 0x00, 0xff, 0xff));
	// 	lines.draw(glm::vec3(Game::ArenaMax.x, Game::ArenaMin.y, 0.0f), glm::vec3(Game::ArenaMax.x, Game::ArenaMax.y, 0.0f), glm::u8vec4(0xff, 0x00, 0xff, 0xff));

	// 	for (auto const &player : game.players) {
	// 		glm::u8vec4 col = glm::u8vec4(player.color.x*255, player.color.y*255, player.color.z*255, 0xff);
	// 		if (&player == &game.players.front()) {
	// 			//mark current player (which server sends first):
	// 			lines.draw(
	// 				glm::vec3(player.position + Game::PlayerRadius * glm::vec2(-0.5f,-0.5f), 0.0f),
	// 				glm::vec3(player.position + Game::PlayerRadius * glm::vec2( 0.5f, 0.5f), 0.0f),
	// 				col
	// 			);
	// 			lines.draw(
	// 				glm::vec3(player.position + Game::PlayerRadius * glm::vec2(-0.5f, 0.5f), 0.0f),
	// 				glm::vec3(player.position + Game::PlayerRadius * glm::vec2( 0.5f,-0.5f), 0.0f),
	// 				col
	// 			);
	// 		}
	// 		for (uint32_t a = 0; a < circle.size(); ++a) {
	// 			lines.draw(
	// 				glm::vec3(player.position + Game::PlayerRadius * circle[a], 0.0f),
	// 				glm::vec3(player.position + Game::PlayerRadius * circle[(a+1)%circle.size()], 0.0f),
	// 				col
	// 			);
	// 		}

	// 		draw_text(player.position + glm::vec2(0.0f, -0.1f + Game::PlayerRadius), player.name, 0.09f);
	// 	}
	// }
	GL_ERRORS();
}

std::list<Scene::Drawable>::iterator PlayMode::create_drawable(Scene &scene, std::list<Scene::Drawable>::iterator it_src) {
	Scene::Transform *src_t = it_src->transform;

	scene.transforms.emplace_back();
	Scene::Transform &dst_t = scene.transforms.back();
	dst_t = *src_t;
	dst_t.parent = nullptr;

	scene.drawables.emplace_back(&dst_t);
    Scene::Drawable &dst_d = scene.drawables.back();
    dst_d = *it_src;
    dst_d.transform = &dst_t;

	return std::prev(scene.drawables.end()); 
}

void PlayMode::delete_drawable(Scene &scene, std::list<Scene::Drawable>::iterator it) {
	Scene::Transform *t = it->transform;
	scene.drawables.erase(it);
	
	for (auto ti = scene.transforms.begin(); ti != scene.transforms.end(); ti++) {
		if (&*ti == t) {
			scene.transforms.erase(ti);
			break;
		}
	}
}
