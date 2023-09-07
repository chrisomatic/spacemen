
#include "main.h"
#include "projectile.h"
#include "player.h"
#include "core/gfx.h"
#include "core/window.h"

Player players[MAX_PLAYERS] = {0};
Player* player = &players[0];

int player_image = -1;
int player_count = 1;

static void player_reset(Player* p);

void player_init_local()
{
    window_controls_add_key(&player->actions[PLAYER_ACTION_FORWARD].state, GLFW_KEY_W);
    window_controls_add_key(&player->actions[PLAYER_ACTION_BACKWARD].state, GLFW_KEY_S);
    window_controls_add_key(&player->actions[PLAYER_ACTION_LEFT].state, GLFW_KEY_A);
    window_controls_add_key(&player->actions[PLAYER_ACTION_RIGHT].state, GLFW_KEY_D);
    window_controls_add_key(&player->actions[PLAYER_ACTION_SHOOT].state, GLFW_KEY_SPACE);
    window_controls_add_key(&player->actions[PLAYER_ACTION_SHIELD].state, GLFW_KEY_F);
    // window_controls_add_key(&player->actions[PLAYER_ACTION_DEBUG].state, GLFW_KEY_F3);
    window_controls_add_key(&player->actions[PLAYER_ACTION_RESET].state, GLFW_KEY_R);
    window_controls_add_key(&player->actions[PLAYER_ACTION_PAUSE].state, GLFW_KEY_P);

    player->active = true;
}

void player_init(Player* p)
{
    //if(role != ROLE_SERVER)
    //{
        if(player_image == -1)
            player_image = gfx_load_image("src/img/spaceship.png", false, true, 32, 32);
    //}

    memset(p,0, sizeof(Player));

    p->pos.x = 100.0;
    p->pos.y = 100.0;
    p->vel.x = 0.0;
    p->vel.y = 0.0;
    p->angle_deg = 0.0;
    p->accel_factor = 10.0;
    p->turn_rate = 5.0;
    p->velocity_limit = 500.0;
    p->energy = MAX_ENERGY/2.0;
    p->force_field = false;
    p->hp_max = 100.0;
    p->hp = p->hp_max;

    p->hit_box.x = p->pos.x;
    p->hit_box.y = p->pos.y;
    GFXImage* img = &gfx_images[player_image];
    float wh = MAX(img->element_width, img->element_height)*0.9;
    p->hit_box.w = wh;
    p->hit_box.h = wh;

    memcpy(&p->hit_box_prior, &p->hit_box, sizeof(Rect));
}

void player_activate(Player* p)
{
    p->active = true;
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

    if(role == ROLE_LOCAL)
    {
        if(p->actions[PLAYER_ACTION_PAUSE].toggled_on)
            paused = !paused;
    }

    if(paused) return;

    bool fwd   = p->actions[PLAYER_ACTION_FORWARD].state;
    bool bkwd  = p->actions[PLAYER_ACTION_BACKWARD].state;
    bool left  = p->actions[PLAYER_ACTION_LEFT].state;
    bool right = p->actions[PLAYER_ACTION_RIGHT].state;

    if(fwd)
    {
        float angle = RAD(p->angle_deg);

        if(fwd)
        {
            p->vel.x += p->accel_factor*cos(angle);
            p->vel.y -= p->accel_factor*sin(angle);
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

    if(bkwd)
    {
        p->vel.x *= (55 * delta_t);
        p->vel.y *= (55 * delta_t);
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
        player_reset(p);
    }

    player_update_hit_box(p);

    Vector2f adj = limit_rect_pos(&world_box, &p->hit_box);

    if(FEQ0(adj.x) && FEQ0(adj.y))
    {
        // no collision
    }
    else
    {
        p->pos.x += adj.x;
        p->pos.y += adj.y;

        float x0 = p->pos.x;
        float y0 = p->pos.y;

        float x1 = x0 + p->vel.x;
        float y1 = y0 + p->vel.y;

        // could be useful to include in player struct
        float vel_angle = calc_angle_rad(x0, y0, x1, y1);

        float xa = cos(vel_angle);
        float ya = sin(vel_angle);

        float new_angle = 0;
        if(!FEQ0(adj.x))
        {
            new_angle = PI - vel_angle;
        }
        else
        {
            new_angle = PI*2 - vel_angle;
        }

        if(!FEQ0(xa))
        {
            float xcomp = p->vel.x / cos(vel_angle);
            p->vel.x = xcomp*cos(new_angle);
        }

        if(!FEQ0(ya))
        {
            float ycomp = p->vel.y / sin(vel_angle);
            p->vel.y = ycomp*sin(new_angle);
        }

    }

    float ff_energy = 70.0*delta_t;
    if(p->force_field)
    {
        player_add_energy(p, -ff_energy);
        if(p->energy <= ff_energy)
        {
            p->force_field = false;
        }
    }

    if(p->actions[PLAYER_ACTION_SHIELD].toggled_on)
    {
        if(!p->force_field && p->energy >= ff_energy)
        {
            p->force_field = true;
        }
        else
        {
            p->force_field = false;
        }
    }

    const float pcooldown = 0.1; //seconds
    if(p->actions[PLAYER_ACTION_SHOOT].toggled_on)
    {
        projectile_add(p, 0);
        p->proj_cooldown = pcooldown;
    }
    else if(p->actions[PLAYER_ACTION_SHOOT].state)
    {
        p->proj_cooldown -= delta_t;
        if(p->proj_cooldown <= 0.0)
        {
            projectile_add(p, 0);
            p->proj_cooldown = pcooldown;
        }
    }

    float energy = 16.0*delta_t;
    player_add_energy(p, energy);

    // printf("energy: %.2f\n", p->energy);
}

void player_add_energy(Player* p, float e)
{
    p->energy = RANGE(p->energy + e, 0.0, MAX_ENERGY);
}

void player_update_hit_box(Player* p)
{
    memcpy(&p->hit_box_prior, &p->hit_box, sizeof(Rect));
    p->hit_box.x = p->pos.x;
    p->hit_box.y = p->pos.y;
}

void player_die(Player* p)
{
    player_reset(p);
}

void player_hurt(Player* p, float damage)
{
    p->hp -= damage;
    if(p->hp <= 0)
    {
        p->hp = 0;
        player_die(p);
    }
}

void player_draw(Player* p)
{
    if(!p->active) return;
    gfx_draw_image(player_image, 0, p->pos.x,p->pos.y, player->color, 1.0, p->angle_deg, 1.0, true, true);

    if(p->force_field)
    {
        gfx_draw_circle(p->pos.x, p->pos.y, p->hit_box.w+3, COLOR_BLUE, 0.2, true, true);
    }

    float name_scale = 0.15;
    Vector2f title_size = gfx_string_get_size(name_scale, player->name);
    gfx_draw_string(p->pos.x - p->hit_box.w/2.0, p->pos.y + p->hit_box.h/2.0 + 5, p->color, name_scale, 0.0, 0.5, true, false, p->name);

    if(game_debug_enabled)
    {
        gfx_draw_rect(&p->hit_box_prior, COLOR_GREEN, 0, 1.0, 1.0, false, true);
        gfx_draw_rect(&p->hit_box, COLOR_BLUE, 0, 1.0, 1.0, false, true);

        float x0 = p->pos.x;
        float y0 = p->pos.y;

        // Vector2f vn = {.x = p->vel.x, .y = p->vel.y};
        // normalize(&vn);
        // float x1 = x0 + vn.x*50;
        // float y1 = y0 + vn.y*50;

        float x1 = x0 + p->vel.x;
        float y1 = y0 + p->vel.y;

        gfx_add_line(x0, y0, x1, y1, COLOR_RED);
    }

    if(p == player)
    {
        // draw hp
        float hp_bar_width  = view_width/2.0;
        float red_width = hp_bar_width*(p->hp/p->hp_max);
        float hp_bar_height = 15.0;
        // float hp_bar_padding = 4.0;

        float hp_bar_x = (view_width)/2.0;
        float hp_bar_y = view_height-hp_bar_height-5.0;

        gfx_draw_rect_xywh(hp_bar_x, hp_bar_y, hp_bar_width, hp_bar_height, COLOR_BLACK,0.0,1.0,0.7,true,false);
        gfx_draw_rect_xywh(hp_bar_x + red_width/2.0 - hp_bar_width/2.0, hp_bar_y, red_width, hp_bar_height, 0x00CC0000,0.0,1.0,0.4,true,false);
        gfx_draw_rect_xywh(hp_bar_x, hp_bar_y, hp_bar_width, hp_bar_height, COLOR_BLACK,0.0,1.0,0.7,false,false); // border

        // draw energy
        float energy_width = hp_bar_width*(p->energy/MAX_ENERGY);
        gfx_draw_rect_xywh(hp_bar_x + energy_width/2.0 - hp_bar_width/2.0, hp_bar_y + hp_bar_height - 5.0, energy_width, hp_bar_height/4.0, COLOR_YELLOW, 0.0,1.0,0.4,true,false);


        Vector2f l = gfx_draw_string(10.0, view_height-30.0, COLOR_BLACK, 0.15, 0.0, 1.0, true, false, "%8.2f, %8.2f", p->vel.x, p->vel.y);
        gfx_draw_string(10.0, view_height-30.0+l.y, COLOR_BLACK, 0.15, 0.0, 1.0, true, false, "%8.2f, %8.2f", p->pos.x, p->pos.y);
    }
}

void player_handle_net_inputs(Player* p, double delta_t)
{
    // handle input
    memcpy(&p->input_prior, &p->input, sizeof(NetPlayerInput));

    p->input.delta_t = delta_t;

    p->input.keys = 0;

    for(int i = 0; i < PLAYER_ACTION_MAX; ++i)
    {
        if(p->actions[i].state)
        {
            p->input.keys |= (1<<i);
        }
    }

    if(p->input.keys != p->input_prior.keys)
    {
        net_client_add_player_input(&p->input);
    }
}

void player_lerp(Player* p, double delta_t)
{
    if(!p->active) return;

    p->lerp_t += delta_t;

    float tick_time = 1.0/TICK_RATE;
    float t = (p->lerp_t / tick_time);

    Vector2f lp = lerp2f(&p->server_state_prior.pos,&p->server_state_target.pos,t);
    p->pos.x = lp.x;
    p->pos.y = lp.y;

    p->angle_deg = lerp(p->server_state_prior.angle,p->server_state_target.angle,t);
    p->energy = lerp(p->server_state_prior.energy, p->server_state_target.energy,t);
    p->hp = lerp(p->server_state_prior.hp, p->server_state_target.hp, t);

}

static void player_reset(Player* p)
{
    p->pos.x = VIEW_WIDTH/2.0;
    p->pos.y = VIEW_HEIGHT/2.0;
    p->vel.x = 0.0;
    p->vel.y = 0.0;
    p->energy = MAX_ENERGY;
    p->hp = p->hp_max;
}
