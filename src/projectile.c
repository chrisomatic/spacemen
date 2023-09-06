#include "headers.h"
#include "main.h"
#include "math2d.h"
#include "window.h"
#include "gfx.h"
#include "log.h"
#include "player.h"
#include "projectile.h"

Projectile projectiles[MAX_PROJECTILES];
glist* plist = NULL;

static int projectile_image;
static uint16_t id_counter = 0;

static void projectile_remove(int index)
{
    list_remove(plist, index);
}

static uint16_t get_id()
{
    if(id_counter >= 65535)
        id_counter = 0;

    return id_counter++;
}

void projectile_init()
{
    plist = list_create((void*)projectiles, MAX_PROJECTILES, sizeof(Projectile));
    projectile_image = gfx_load_image("src/img/laser.png", false, false, 10, 3);
}

void projectile_add(Player* p, float angle_offset)
{
    Projectile proj = {0};

    float energy_usage = 10.0;
    if(p->energy < energy_usage) return;
    player_add_energy(p, -energy_usage);

    proj.id = get_id();
    proj.shooter = p;

    proj.dead = false;
    proj.angle_deg = p->angle_deg;
    proj.damage = 10.0;

    proj.pos.x = p->pos.x;
    proj.pos.y = p->pos.y;
    float angle = RAD(proj.angle_deg);
    float speed = 400.0;
    proj.vel.x = (speed)*cosf(angle) + p->vel.x;
    proj.vel.y = (speed)*sinf(angle) - p->vel.y;

    proj.time = 0.0;
    proj.ttl  = 5.0;

    proj.hit_box.x = proj.pos.x;
    proj.hit_box.y = proj.pos.y;
    GFXImage* img = &gfx_images[projectile_image];
    float wh = MAX(img->element_width, img->element_height);
    proj.hit_box.w = wh;
    proj.hit_box.h = wh;

    memcpy(&proj.hit_box_prior, &proj.hit_box, sizeof(Rect));

    list_add(plist, (void*)&proj);
}


void projectile_update(float delta_t)
{
    // printf("projectile update\n");

    for(int i = plist->count - 1; i >= 0; --i)
    {
        Projectile* proj = &projectiles[i];

        if(proj->dead)
            continue;

        if(proj->time >= proj->ttl)
        {
            proj->dead = true;
            continue;
        }

        float _dt = RANGE(proj->ttl - proj->time, 0.0, delta_t);
        // printf("%3d %.4f\n", i, delta_t);

        proj->time += _dt;

        // proj->prior_pos.x = proj->pos.x;
        // proj->prior_pos.y = proj->pos.y;

        proj->pos.x += _dt*proj->vel.x;
        proj->pos.y -= _dt*proj->vel.y; // @minus

        projectile_update_hit_box(proj);
    }

    // int count = 0;
    for(int i = plist->count - 1; i >= 0; --i)
    {
        if(projectiles[i].dead)
        {
            // count++;
            projectile_remove(i);
        }
    }
    // if(count > 0) printf("removed %d\n", count);

}

void projectile_update_hit_box(Projectile* proj)
{
    memcpy(&proj->hit_box_prior, &proj->hit_box, sizeof(Rect));
    proj->hit_box.x = proj->pos.x;
    proj->hit_box.y = proj->pos.y;
    // print_rect(&proj->hit_box);
}

void projectile_lerp(Projectile* p, double delta_t)
{
    p->lerp_t += delta_t;

    float tick_time = 1.0/TICK_RATE;
    float t = (p->lerp_t / tick_time);

    if((p->server_state_prior.pos.x == 0.0 && p->server_state_prior.pos.y == 0.0) || p->server_state_prior.id != p->server_state_target.id)
    {
        // new projectile, set position and id directly
        p->server_state_prior.id = p->server_state_target.id;
        p->id = p->server_state_target.id;
        memcpy(&p->server_state_prior.pos, &p->server_state_target.pos, sizeof(Vector2f));
    }

    Vector2f lp = lerp2f(&p->server_state_prior.pos,&p->server_state_target.pos,t);
    p->pos.x = lp.x;
    p->pos.y = lp.y;
    //TODO
    p->hit_box.w = 10;
    p->hit_box.h = 10;

    p->angle_deg = lerp(p->server_state_prior.angle,p->server_state_target.angle,t);
}

void projectile_handle_collisions(float delta_t)
{
    for(int i = plist->count - 1; i >= 0; --i)
    {
        Projectile* p = &projectiles[i];
        if(p->dead) continue;

        for(int j = 0; j < MAX_PLAYERS; ++j)
        {
            if(!players[j].active) continue;

            if(p->shooter == &players[j]) continue;

            bool hit = are_rects_colliding(&p->hit_box_prior, &p->hit_box, &players[j].hit_box);
            if(hit)
            {
                printf("player %d got hit!\n", j);
                player_hurt(&players[j], p->damage);
                p->dead = true;
                break;
            }
        }
    }
}

void projectile_draw(Projectile* proj)
{
    gfx_draw_image(projectile_image, 0, proj->pos.x, proj->pos.y, COLOR_TINT_NONE, 1.0, proj->angle_deg, 1.0, true, true);

    if(game_debug_enabled)
    {
        gfx_draw_rect(&proj->hit_box_prior, COLOR_GREEN, 0, 1.0, 1.0, false, true);
        gfx_draw_rect(&proj->hit_box, COLOR_BLUE, 0, 1.0, 1.0, false, true);
    }
}
