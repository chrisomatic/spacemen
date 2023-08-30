#pragma once

#include "player.h"
#include "glist.h"

#define MAX_PROJECTILES 1024

typedef struct
{
    Vector2f pos;
    Vector2f vel;
    float angle_deg;

    float time;
    float ttl;
    bool dead;
} Projectile;

extern Projectile projectiles[MAX_PROJECTILES];
extern glist* plist;

void projectile_init();
void projectile_add(Player* p, float angle_offset);

void projectile_update(float delta_t);
void projectile_draw(Projectile* proj);
