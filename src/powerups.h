#pragma once

#include "player.h"

#define MAX_POWERUPS 100

typedef void (*powerup_func)(Player* player, bool picked_up, bool expired);

typedef enum
{
    POWERUP_TYPE_NONE,
    POWERUP_TYPE_HEALTH,
    POWERUP_TYPE_MAX,
} PowerupType;

typedef struct
{
    Vector2f pos;
    float lifetime;
    float lifetime_max;
    PowerupType type;
    powerup_func func;
    bool picked_up;
    bool temporary;
    float duration;
} Powerup;


void powerups_init();
void powerups_add(float x, float y, PowerupType type);
void powerups_update(double dt);
void powerups_draw();
