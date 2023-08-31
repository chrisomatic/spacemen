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

    proj.dead = false;
    proj.angle_deg = p->angle_deg;

    proj.pos.x = p->pos.x;
    proj.pos.y = p->pos.y;
    float angle = RAD(proj.angle_deg);
    float speed = 100.0;
    proj.vel.x = speed*cosf(angle);
    proj.vel.y = speed*sinf(angle);

    proj.time = 0.0;
    proj.ttl  = 5.0;   //TODO

    // for(int i = 0; i < 100; ++i)
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

        proj->pos.x += _dt*proj->vel.x;
        proj->pos.y -= _dt*proj->vel.y; // @minus

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

void projectile_draw(Projectile* proj)
{
    gfx_draw_image(projectile_image, 0, proj->pos.x, proj->pos.y, COLOR_TINT_NONE, 1.0, proj->angle_deg, 1.0, true, true);
}
