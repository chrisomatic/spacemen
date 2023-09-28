#include "core/headers.h"
#include "core/gfx.h"
#include "core/glist.h"
#include "main.h"
#include "powerups.h"

static Powerup powerups[MAX_POWERUPS] = {0};
static int powerups_img = -1;

static glist* powerup_list = NULL;

static void powerups_remove(int index)
{
    list_remove(powerup_list, index);
}

static float powerup_spawn_time;
static float powerup_spawn_time_target;

static const int min_spawn_time = 1;
static const int max_spawn_time = 5;

static float get_next_powerups_spawn_time()
{
    int min = min_spawn_time*10;
    int max = max_spawn_time*10;

    return (float)(((rand() % (max-min)) + min)/10.0);
}

void powerups_init()
{
    if(powerups_img == -1)
    {
        powerups_img = gfx_load_image("src/img/powerups.png", false, false, 32, 32);
    }

    powerup_list = list_create((void*)powerups, MAX_POWERUPS, sizeof(Powerup));

    powerup_spawn_time = 0.0;
    powerup_spawn_time_target = get_next_powerups_spawn_time();
}

void powerups_add(float x, float y, PowerupType type)
{
    Powerup pup = {0};

    pup.pos.x = x;
    pup.pos.y = y;
    pup.lifetime_max = 10.0;
    pup.lifetime = 0.0;
    pup.type = type;
    pup.picked_up = false;

    list_add(powerup_list, (void*)&pup);
}

void powerups_update(double dt)
{
    // handle spawning powerups
    powerup_spawn_time += dt;
    if(powerup_spawn_time >= powerup_spawn_time_target)
    {
        powerup_spawn_time = 0.0;
        powerup_spawn_time_target = get_next_powerups_spawn_time();

        float x = rand() % (int)(world_box.w - 32) + 16;
        float y = rand() % (int)(world_box.h - 32) + 16;

        powerups_add(x,y,POWERUP_TYPE_HEALTH);
    }

    for(int i = powerup_list->count -1; i >= 0; --i)
    {
        Powerup* pup = &powerups[i];

        if(!pup->picked_up)
        {
            pup->lifetime += dt;
            if(pup->lifetime >= pup->lifetime_max)
            {
                list_remove(powerup_list, i);
            }
        }
    }
}

void powerups_draw()
{
    for(int i = 0; i < powerup_list->count; ++i)
    {
        Powerup* pup = &powerups[i];

        if(!pup->picked_up)
        {
            gfx_draw_image(powerups_img, pup->type, pup->pos.x, pup->pos.y + 5.0*sin(pup->lifetime*10), COLOR_TINT_NONE, 1.0, 0.0, 1.0, false, true);
        }
    }
}
