#pragma once

#include <glm/glm.hpp>

#include <string>
#include <list>
#include <random>

struct Connection;

//Game state, separate from rendering.

//Currently set up for a "client sends controls" / "server sends whole state" situation.

struct Spotlight {
	glm::vec3 pos = glm::vec3(0.0f);
	glm::vec3 dir = glm::vec3(0.0f, 0.0f, 0.0f);
	glm::vec3 energy = glm::vec3(20.0f);
	float cutoff = glm::radians(20.0f);
};

enum class Message : uint8_t {
	C2S_Controls = 1, //Greg!
	S2C_State = 's',
	//...
};

//used to represent a control input:
struct Button {
	uint8_t downs = 0; //times the button has been pressed
	bool pressed = false; //is the button pressed now
};

//state of one player in the game:
struct Player {
	//player inputs (sent from client):
	struct Controls {
		Button left, right, up, down, jump;

		void send_controls_message(Connection *connection) const;

		//returns 'false' if no message or not a controls message,
		//returns 'true' if read a controls message,
		//throws on malformed controls message
		bool recv_controls_message(Connection *connection);
	} controls;

	//player state (sent from server):
	glm::vec2 position = glm::vec2(0.0f, 0.0f);
	glm::vec2 velocity = glm::vec2(0.0f, 0.0f);

	glm::vec3 color = glm::vec3(1.0f, 1.0f, 1.0f);
	std::string name = "";

	enum class Role : uint8_t { Seeker = 0, Hider = 1 };
	Role role;

	bool is_ready;
	uint8_t player_id;
	bool is_caught;
};

struct Game {
	std::list< Player > players; //(using list so they can have stable addresses)
	Player *spawn_player(); //add player the end of the players list (may also, e.g., play some spawn anim)
	void remove_player(Player *); //remove player from game (may also, e.g., play some despawn anim)

	std::mt19937 mt; //used for spawning players
	uint32_t next_player_number = 1; //used for naming players

	Game();

	//state update function:
	void update(float elapsed);

	//constants:
	//the update rate on the server:
	inline static constexpr float Tick = 1.0f / 30.0f;

	//arena size:
	// inline static constexpr glm::vec2 ArenaMin = glm::vec2(-0.75f, -1.0f);
	// inline static constexpr glm::vec2 ArenaMax = glm::vec2( 0.75f,  1.0f);

	inline static constexpr glm::vec2 ArenaMin = glm::vec2(-10.0f, -10.0f);
	inline static constexpr glm::vec2 ArenaMax = glm::vec2( 10.0f,  10.0f);

	//player constants:
	// inline static constexpr float PlayerRadius = 0.06f;
	inline static constexpr float PlayerRadius = 0.8f;
	// inline static constexpr float PlayerSpeed = 2.0f;
	inline static constexpr float PlayerSpeed = 6.0f;
	inline static constexpr float PlayerAccelHalflife = 0.25f;
	

	//---- communication helpers ----

	//used by client:
	//set game state from data in connection buffer
	// (return true if data was read)
	bool recv_state_message(Connection *connection);

	//used by server:
	//send game state.
	//  Will move "connection_player" to the front of the front of the sent list.
	void send_state_message(Connection *connection, Player *connection_player = nullptr) const;

	Spotlight spotlight;
	float angle = 0.0f;
	float speed = 1.0f;

	enum class GameState : uint8_t {BeforeStart = 0, Playing = 1, SeekerWin = 2, HiderWin = 3};
	GameState game_state;
	float timer = 60.0f;
	bool all_is_ready;

	int hider_count = 0;
	int caught_hider_count = 0;
	uint8_t current_player_id = 0;
};
