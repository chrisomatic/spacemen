
#include "main.h"
#include "projectile.h"
#include "player.h"
#include "core/gfx.h"
#include "core/window.h"

Player players[MAX_PLAYERS] = {0};
Player* player = &players[0];

int player_image;
int player_count = 1;

void player_init(Player* p)
{
    if(role != ROLE_SERVER)
    {
        player_image = gfx_load_image("src/img/spaceship.png", false, true, 32, 32);
    }

    p->active = true;
    p->pos.x = 100.0;
    p->pos.y = 100.0;
    p->vel.x = 0.0;
    p->vel.y = 0.0;
    p->angle_deg = 0.0;
    p->accel_factor = 10.0;
    p->turn_rate = 5.0;
    p->velocity_limit = 500.0;
    p->energy = MAX_ENERGY/2.0;

    player->hit_box.x = player->pos.x;
    player->hit_box.y = player->pos.y;
    GFXImage* img = &gfx_images[player_image];
    float wh = MAX(img->element_width, img->element_height)*0.9;
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
    p->energy = MAX_ENERGY;

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
        p->pos.x = VIEW_WIDTH/2.0;
        p->pos.y = VIEW_HEIGHT/2.0;
        p->vel.x = 0.0;
        p->vel.y = 0.0;
        p->energy = MAX_ENERGY;
    }

    memcpy(&p->hit_box_prior, &p->hit_box, sizeof(Rect));

    p->hit_box.x = p->pos.x;
    p->hit_box.y = p->pos.y;

    const float pcooldown = 0.1; //seconds
    if(p->actions[PLAYER_ACTION_SHOOT].toggled_on)
    {
        projectile_add(player, 0);
        // energy = -5.0;
        p->proj_cooldown = pcooldown;
    }
    else if(p->actions[PLAYER_ACTION_SHOOT].state)
    {
        p->proj_cooldown -= delta_t;
        if(p->proj_cooldown <= 0.0)
        {
            projectile_add(player, 0);
            // energy = -5.0;
            p->proj_cooldown = pcooldown;
        }
    }

    float energy = 6.0*delta_t;    //0.2
    p->energy = RANGE(p->energy + energy, 0.0, MAX_ENERGY);

    // printf("energy: %.2f\n", p->energy);

}

void player_draw(Player* p)
{
    if(!p->active) return;
    gfx_draw_image(player_image, 0, p->pos.x,p->pos.y, COLOR_TINT_NONE, 1.0, p->angle_deg, 1.0, true, true);

    gfx_draw_rect(&p->hit_box_prior, COLOR_GREEN, 0, 1.0, 1.0, false, true);
    gfx_draw_rect(&p->hit_box, COLOR_BLUE, 0, 1.0, 1.0, false, true);


    // draw energy
    float energy_bar_width  = view_width/2.0;
    float red_width = energy_bar_width*(p->energy/MAX_ENERGY);
    float energy_bar_height = 15.0;
    float energy_bar_padding = 4.0;

    float energy_bar_x = (view_width)/2.0;
    float energy_bar_y = view_height-energy_bar_height-5.0;

    gfx_draw_rect_xywh(energy_bar_x, energy_bar_y, energy_bar_width, energy_bar_height, COLOR_BLACK,0.0,1.0,0.7,true,false);
    gfx_draw_rect_xywh(energy_bar_x + red_width/2.0 - energy_bar_width/2.0, energy_bar_y, red_width, energy_bar_height, 0x00CC0000,0.0,1.0,0.4,true,false);
    gfx_draw_rect_xywh(energy_bar_x, energy_bar_y, energy_bar_width, energy_bar_height, COLOR_BLACK,0.0,1.0,0.7,false,false); // border

    Vector2f l = gfx_draw_string(10.0, view_height-30.0, COLOR_BLACK, 0.15, 0.0, 1.0, true, false, "%8.2f, %8.2f", p->vel.x, p->vel.y);
    gfx_draw_string(10.0, view_height-30.0+l.y, COLOR_BLACK, 0.15, 0.0, 1.0, true, false, "%8.2f, %8.2f", p->pos.x, p->pos.y);

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

    if(p->input.keys != 0x0 && p->input.keys != p->input_prior.keys)
    {
        net_client_add_player_input(&p->input);

        /*
        if(net_client_get_input_count() >= 3) // @HARDCODED 3
        {
            // add position, angle to predicted player state
            PlayerNetState* state = &p->predicted_states[p->predicted_state_index];

            // circular buffer
            if(p->predicted_state_index == MAX_CLIENT_PREDICTED_STATES -1)
            {
                // shift
                for(int i = 1; i <= MAX_CLIENT_PREDICTED_STATES -1; ++i)
                {
                    memcpy(&p->predicted_states[i-1],&p->predicted_states[i],sizeof(PlayerNetState));
                }
            }
            else if(p->predicted_state_index < MAX_CLIENT_PREDICTED_STATES -1)
            {
                p->predicted_state_index++;
            }

            state->associated_packet_id = net_client_get_latest_local_packet_id();
            state->pos.x = p->pos.x;
            state->pos.y = p->pos.y;
            state->angle = p->angle;
        }
        */
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

}
