#pragma once

#include "player.h"
#include "glist.h"

#define MAX_PROJECTILES 256

typedef struct
{
    uint16_t id;
    Vector2f pos;
    Vector2f vel;
    float angle_deg;

    Rect hit_box;
    Rect hit_box_prior;

    Player* shooter;

    float damage;
    float time;
    float ttl;
    bool dead;
    
    // client-side interpolation
    float lerp_t;
    ObjectState server_state_target;
    ObjectState server_state_prior;

} Projectile;

extern Projectile projectiles[MAX_PROJECTILES];
extern glist* plist;

void projectile_init();
void projectile_clear_all();
void projectile_add(Player* p, float angle_offset, float energy_usage);
void projectile_update_hit_box(Projectile* proj);
void projectile_lerp(Projectile* p, double delta_t);
void projectile_update(float delta_t);
void projectile_handle_collisions(float delta_t);
void projectile_draw(Projectile* proj);
