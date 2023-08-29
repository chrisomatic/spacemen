#pragma once

#include <stdbool.h>

enum PlayerActions
{
    PLAYER_ACTION_ACC,
    PLAYER_ACTION_LEFT,
    PLAYER_ACTION_RIGHT,
    PLAYER_ACTION_SHOOT,
    PLAYER_ACTION_SHIELD,
    PLAYER_ACTION_DEBUG,
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
    float vel_x, vel_y;
    float x,y;
    float rotation;
    PlayerAction actions[PLAYER_ACTION_MAX];

} Player;

void player_init();
void player_update(double delta_t);
