
#include "main.h"
#include "player.h"
#include "core/gfx.h"
#include "core/window.h"

Player players[MAX_PLAYERS] = {0};
Player* player = &players[0];

int player_image;
int player_count = 1;

void player_init()
{
    player_image = gfx_load_image("src/img/spaceship.png", false, true, 32, 32);

    player->active = true;
    player->pos.x = 100.0;
    player->pos.y = 100.0;
    player->vel.x = 0.0;
    player->vel.y = 0.0;
    player->angle_deg = 0.0;
    player->accel_factor = 10.0;
    player->turn_rate = 5.0;
    player->velocity_limit = 500.0;

    window_controls_add_key(&player->actions[PLAYER_ACTION_FORWARD].state, GLFW_KEY_W);
    window_controls_add_key(&player->actions[PLAYER_ACTION_BACKWARD].state, GLFW_KEY_S);
    window_controls_add_key(&player->actions[PLAYER_ACTION_LEFT].state, GLFW_KEY_A);
    window_controls_add_key(&player->actions[PLAYER_ACTION_RIGHT].state, GLFW_KEY_D);
    window_controls_add_key(&player->actions[PLAYER_ACTION_SHOOT].state, GLFW_KEY_SPACE);
    window_controls_add_key(&player->actions[PLAYER_ACTION_SHIELD].state, GLFW_KEY_J);
    window_controls_add_key(&player->actions[PLAYER_ACTION_DEBUG].state, GLFW_KEY_F2);
}

void player_update(Player* p, double delta_t)
{
    for(int i = 0; i < PLAYER_ACTION_MAX; ++i)
    {
        PlayerAction* pa = &player->actions[i];
        if(pa->state && !pa->prior_state)
        {
            pa->toggled_on = true;
        }
        else
        {
            pa->toggled_on = false;
        }

        if(!pa->state && pa->prior_state)
        {
            pa->toggled_off = true;
        }
        else
        {
            pa->toggled_off = false;
        }

        pa->prior_state = pa->state;
    }

    if(player->actions[PLAYER_ACTION_DEBUG].toggled_on)
        debug_enabled = !debug_enabled;

    bool fwd   = player->actions[PLAYER_ACTION_FORWARD].state;
    bool bkwd  = player->actions[PLAYER_ACTION_BACKWARD].state;
    bool left  = player->actions[PLAYER_ACTION_LEFT].state;
    bool right = player->actions[PLAYER_ACTION_RIGHT].state;

    if(fwd || bkwd)
    {
        float angle = RAD(player->angle_deg);

        if(fwd)
        {
            player->vel.x += player->accel_factor*cos(angle);
            player->vel.y -= player->accel_factor*sin(angle);
        }

        if(bkwd)
        {
            player->vel.x -= player->accel_factor*cos(angle);
            player->vel.y += player->accel_factor*sin(angle);
        }

        float D = player->velocity_limit;
        float mag = magn(player->vel);

        if(mag > D)
        {
            // scale vel back to velocity limit
            float factor = D / mag;

            player->vel.y *= factor;
            player->vel.x *= factor;
        }

    }

    player->pos.x += player->vel.x*delta_t;
    player->pos.y += player->vel.y*delta_t;

    if(left)
    {
        player->angle_deg += player->turn_rate;
    }

    if(right)
    {
        player->angle_deg -= player->turn_rate;
    }
}

void player_draw()
{
    gfx_draw_image(player_image, 0, player->pos.x,player->pos.y, COLOR_TINT_NONE, 1.0, player->angle_deg, 1.0, true, true);
}
