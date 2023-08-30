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
    proj.ttl  = 10.0;   //TODO

    list_add(plist, (void*)&proj);
}


void projectile_update(float delta_t)
{
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

        delta_t = RANGE(delta_t, 0.0, proj->ttl - proj->time);

        proj->time += delta_t;

        proj->pos.x += delta_t*proj->vel.x;
        proj->pos.y -= delta_t*proj->vel.y; // @minus

    }

    for(int i = plist->count - 1; i >= 0; --i)
    {
        if(projectiles[i].dead)
        {
            projectile_remove(i);
        }
    }

}

void projectile_draw(Projectile* proj)
{
    gfx_draw_image(projectile_image, 0, proj->pos.x, proj->pos.y, COLOR_TINT_NONE, 1.0, proj->angle_deg, 1.0, true, true);
}
