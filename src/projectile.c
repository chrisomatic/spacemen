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

static void projectile_remove(int index)
{
    list_remove(plist, index);
}

void projectile_init()
{
    plist = list_create((void*)projectiles, MAX_PROJECTILES, sizeof(Projectile));
    projectile_image = gfx_load_image("src/img/laser.png", false, false, 10, 3);
}

void projectile_add(Player* p, float angle_offset)
{
    Projectile proj = {0};

    proj.shooter = p;

    proj.dead = false;
    proj.angle_deg = p->angle_deg;

    proj.pos.x = p->pos.x;
    proj.pos.y = p->pos.y;
    float angle = RAD(proj.angle_deg);
    float speed = 100.0;
    proj.vel.x = (speed)*cosf(angle);// + p->vel.x;
    proj.vel.y = (speed)*sinf(angle);// - p->vel.y;

    proj.time = 0.0;
    proj.ttl  = 5.0;   //TODO

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

        memcpy(&proj->hit_box_prior, &proj->hit_box, sizeof(Rect));
        proj->hit_box.x = proj->pos.x;
        proj->hit_box.y = proj->pos.y;
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
                p->dead = true;
                break;
            }
        }
    }
}

void projectile_draw(Projectile* proj)
{
    gfx_draw_image(projectile_image, 0, proj->pos.x, proj->pos.y, COLOR_TINT_NONE, 1.0, proj->angle_deg, 1.0, true, true);

    gfx_draw_rect(&proj->hit_box_prior, COLOR_GREEN, 0, 1.0, 1.0, false, true);
    gfx_draw_rect(&proj->hit_box, COLOR_BLUE, 0, 1.0, 1.0, false, true);
}
