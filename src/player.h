#pragma once

#include <stdbool.h>

#define MAX_PLAYERS 8

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

    Vector2f pos;
    Vector2f vel;
    Rect hit_box;
    Rect hit_box_prior;
    float velocity_limit;
    float accel_factor;
    float turn_rate;
    float angle_deg;
    PlayerAction actions[PLAYER_ACTION_MAX];

    float proj_cooldown;
} Player;

extern Player players[MAX_PLAYERS];
extern Player* player;
extern int player_count;

void player_init();
void player_init_other(int index);
void player_update(Player* p, double delta_t);
void player_draw(Player* p);
