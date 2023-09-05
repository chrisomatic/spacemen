#pragma once

#include <stdbool.h>
#include "net.h"

#define MAX_PLAYERS 8

#define MAX_ENERGY  300


enum PlayerActions
{
    PLAYER_ACTION_FORWARD,
    PLAYER_ACTION_BACKWARD,
    PLAYER_ACTION_LEFT,
    PLAYER_ACTION_RIGHT,
    PLAYER_ACTION_SHOOT,
    PLAYER_ACTION_SHIELD,
    PLAYER_ACTION_DEBUG,
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

    PlayerAction actions[PLAYER_ACTION_MAX];

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
extern int player_count;
extern int player_image;

void player_init(Player* p);
void player_init_local();
void player_update(Player* p, double delta_t);
void player_update_hit_box(Player* p);
void player_draw(Player* p);

// networking
void player_handle_net_inputs(Player* p, double delta_t);
void player_lerp(Player* p, double delta_t);
