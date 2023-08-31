
#include "main.h"
#include "projectile.h"
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

    player->hit_box.x = player->pos.x;
    player->hit_box.y = player->pos.y;
    GFXImage* img = &gfx_images[player_image];
    float wh = MAX(img->element_width, img->element_height);
    player->hit_box.w = wh;
    player->hit_box.h = wh;
    memcpy(&player->hit_box_prior, &player->hit_box, sizeof(Rect));

    window_controls_add_key(&player->actions[PLAYER_ACTION_FORWARD].state, GLFW_KEY_W);
    window_controls_add_key(&player->actions[PLAYER_ACTION_BACKWARD].state, GLFW_KEY_S);
    window_controls_add_key(&player->actions[PLAYER_ACTION_LEFT].state, GLFW_KEY_A);
    window_controls_add_key(&player->actions[PLAYER_ACTION_RIGHT].state, GLFW_KEY_D);
    window_controls_add_key(&player->actions[PLAYER_ACTION_SHOOT].state, GLFW_KEY_SPACE);
    window_controls_add_key(&player->actions[PLAYER_ACTION_SHIELD].state, GLFW_KEY_J);
    window_controls_add_key(&player->actions[PLAYER_ACTION_DEBUG].state, GLFW_KEY_F2);
    window_controls_add_key(&player->actions[PLAYER_ACTION_RESET].state, GLFW_KEY_R);
    window_controls_add_key(&player->actions[PLAYER_ACTION_PAUSE].state, GLFW_KEY_P);
}

void player_init_other(int index)
{
    if(index == 0 || index >= MAX_PLAYERS) return;

    Player* p = &players[index];
    p->active = true;
    p->pos.x = 200.0;
    p->pos.y = 100.0;
    p->vel.x = 0.0;
    p->vel.y = 0.0;
    p->angle_deg = 0.0;
    p->accel_factor = 10.0;
    p->turn_rate = 5.0;
    p->velocity_limit = 500.0;

    p->hit_box.x = p->pos.x;
    p->hit_box.y = p->pos.y;
    GFXImage* img = &gfx_images[player_image];
    float wh = MAX(img->element_width, img->element_height);
    p->hit_box.w = wh;
    p->hit_box.h = wh;
    memcpy(&p->hit_box_prior, &p->hit_box, sizeof(Rect));

    player_count++;
}

void player_update(Player* p, double delta_t)
{
    if(!p->active) return;
    // printf("player update\n");

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

    if(p->actions[PLAYER_ACTION_PAUSE].toggled_on)
        paused = !paused;

    if(paused) return;

    bool fwd   = p->actions[PLAYER_ACTION_FORWARD].state;
    bool bkwd  = p->actions[PLAYER_ACTION_BACKWARD].state;
    bool left  = p->actions[PLAYER_ACTION_LEFT].state;
    bool right = p->actions[PLAYER_ACTION_RIGHT].state;

    if(fwd || bkwd)
    {
        float angle = RAD(p->angle_deg);

        if(fwd)
        {
            p->vel.x += p->accel_factor*cos(angle);
            p->vel.y -= p->accel_factor*sin(angle);
        }

        if(bkwd)
        {
            p->vel.x -= p->accel_factor*cos(angle);
            p->vel.y += p->accel_factor*sin(angle);
        }

        float D = p->velocity_limit;
        float mag = magn(p->vel);

        if(mag > D)
        {
            // scale vel back to velocity limit
            float factor = D / mag;

            p->vel.y *= factor;
            p->vel.x *= factor;
        }

    }

    p->pos.x += p->vel.x*delta_t;
    p->pos.y += p->vel.y*delta_t;

    if(left)
    {
        p->angle_deg += p->turn_rate;
    }

    if(right)
    {
        p->angle_deg -= p->turn_rate;
    }

    if(p->actions[PLAYER_ACTION_RESET].toggled_on)
    {
        p->pos.x = VIEW_WIDTH/2.0;
        p->pos.y = VIEW_HEIGHT/2.0;
        p->vel.x = 0.0;
        p->vel.y = 0.0;
    }

    memcpy(&p->hit_box_prior, &p->hit_box, sizeof(Rect));

    p->hit_box.x = p->pos.x;
    p->hit_box.y = p->pos.y;

    const float pcooldown = 0.1; //seconds
    if(p->actions[PLAYER_ACTION_SHOOT].toggled_on)
    {
        projectile_add(player, 0);
        p->proj_cooldown = pcooldown;
    }
    else if(p->actions[PLAYER_ACTION_SHOOT].state)
    {
        p->proj_cooldown -= delta_t;
        if(p->proj_cooldown <= 0.0)
        {
            projectile_add(player, 0);
            p->proj_cooldown = pcooldown;
        }

    }

}

void player_draw(Player* p)
{
    if(!p->active) return;
    gfx_draw_image(player_image, 0, p->pos.x,p->pos.y, COLOR_TINT_NONE, 1.0, p->angle_deg, 1.0, true, true);

    gfx_draw_rect(&p->hit_box_prior, COLOR_GREEN, 0, 1.0, 1.0, false, true);
    gfx_draw_rect(&p->hit_box, COLOR_BLUE, 0, 1.0, 1.0, false, true);
}
