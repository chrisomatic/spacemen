#pragma once

#include "player.h"

#define MAX_POWERUPS 100

typedef void (*powerup_func)(Player* player, bool expired);

typedef enum
{
    POWERUP_TYPE_NONE,
    POWERUP_TYPE_HEALTH,
    POWERUP_TYPE_INVINCIBILITY,
    POWERUP_TYPE_HEALTH_FULL,
    POWERUP_TYPE_MAX,
} PowerupType;

typedef struct
{
    Vector2f base_pos;
    Vector2f pos;
    float lifetime;
    float lifetime_max;
    PowerupType type;
    powerup_func func;
    bool picked_up;
    bool temporary;
    float duration;
    float duration_max;
    Rect hit_box;
    Player* picked_up_player;
} Powerup;

extern Powerup powerups[MAX_POWERUPS];

void powerups_init();
void powerups_add(float x, float y, PowerupType type);
void powerups_update(double dt);
void powerups_draw();
Powerup* powerups_get_list();
uint8_t powerups_get_count();
void powerups_clear_all();
