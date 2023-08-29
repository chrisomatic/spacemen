
#include "main.h"
#include "player.h"
#include "core/gfx.h"
#include "core/window.h"

Player player = {0};
Player* p = &player;

int player_image;

void player_init()
{
    player_image = gfx_load_image("src/img/spaceship.png", false, true, 32, 32);

    p->x = 100.0;
    p->y = 100.0;
    p->velocity = 0.0;
    p->rotation = 0.0;

    window_controls_add_key(&p->actions[PLAYER_ACTION_ACC].state, GLFW_KEY_W);
    window_controls_add_key(&p->actions[PLAYER_ACTION_LEFT].state, GLFW_KEY_A);
    window_controls_add_key(&p->actions[PLAYER_ACTION_RIGHT].state, GLFW_KEY_D);
    window_controls_add_key(&p->actions[PLAYER_ACTION_SHOOT].state, GLFW_KEY_SPACE);
    window_controls_add_key(&p->actions[PLAYER_ACTION_SHIELD].state, GLFW_KEY_J);
    window_controls_add_key(&p->actions[PLAYER_ACTION_DEBUG].state, GLFW_KEY_F2);
}

void player_update(double delta_t)
{
    for(int i = 0; i < PLAYER_ACTION_MAX; ++i)
    {
        PlayerAction* pa = &p->actions[i];
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

    if(p->actions[PLAYER_ACTION_DEBUG].toggled_on)
        debug_enabled = !debug_enabled;

    bool acc   = p->actions[PLAYER_ACTION_ACC].state;
    bool left  = p->actions[PLAYER_ACTION_LEFT].state;
    bool right = p->actions[PLAYER_ACTION_RIGHT].state;

    p->accel = acc ? 60.0 : 0.0;

    p->velocity += p->accel*delta_t;

    float angle = RAD(p->rotation);

    p->x += p->velocity*cos(angle)*delta_t;
    p->y += p->velocity*sin(angle)*delta_t;

    if(left)
    {
        p->rotation += 5.0;
    }

    if(right)
    {
        p->rotation -= 5.0;
    }
}

void player_draw()
{
    gfx_draw_image(player_image, 0, p->x,p->y, COLOR_TINT_NONE, 1.0, p->rotation, 1.0, true, true);
}
