#pragma once

#include <stdbool.h>
#include "net.h"
#include "particles.h"
#include "settings.h"

#define MAX_PLAYERS 8

#define MAX_ENERGY  300


enum PlayerActions
{
    PLAYER_ACTION_FORWARD,
    PLAYER_ACTION_BACKWARD,
    PLAYER_ACTION_LEFT,
    PLAYER_ACTION_RIGHT,
    PLAYER_ACTION_SCUM, //TODO
    PLAYER_ACTION_SHOOT,
    PLAYER_ACTION_SHIELD,
    PLAYER_ACTION_PAUSE,
    PLAYER_ACTION_RESET,
    PLAYER_ACTION_MAX
};

typedef struct
{
    bool state;
    bool prior_state;
    bool toggled_on;
    bool toggled_off;
} PlayerAction;

typedef struct
{
    bool active;
    bool ai;
    bool invincible;

    uint8_t id;

    bool dead;
    int deaths;
    float hp;
    float hp_max;

    // physics
    Vector2f pos;
    Vector2f vel;
    Rect hit_box;
    Rect hit_box_prior;

    float velocity_limit;
    float accel_factor;
    float turn_rate;
    float angle_deg;

    float proj_cooldown;
    float energy;

    bool force_field;

    Settings settings;

    PlayerAction actions[PLAYER_ACTION_MAX];

    ParticleSpawner* jets;

    // networking
    NetPlayerInput input;
    NetPlayerInput input_prior;

    // client-side interpolation
    float lerp_t;
    ObjectState server_state_target;
    ObjectState server_state_prior;

} Player;

extern Player players[MAX_PLAYERS];
extern Player* player;
extern Player* player2;
extern int player_count;
extern int player_image;
extern char* player_names[MAX_PLAYERS+1];

void players_init();
Player* player_get_by_id(uint8_t id);
void player_set_active_state(uint8_t id, bool active);

void player_init_local();
void player_set_controls();
void player_update(Player* p, double delta_t);
void player_add_energy(Player* p, float e);
void player_update_hit_box(Player* p);
void player_draw(Player* p);
void player_determine_winner();
void player_hurt(Player* p, float damage);
void player_heal(Player* p, float hp);
void player_die(Player* p);
void players_set_ai_state();
void player_reset(Player* p);

// networking
void player_handle_net_inputs(Player* p, double delta_t);
void player_lerp(Player* p, double delta_t);
