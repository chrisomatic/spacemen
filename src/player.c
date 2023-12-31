
#include "main.h"
#include "projectile.h"
#include "player.h"
#include "core/gfx.h"
#include "core/window.h"
#include "core/particles.h"
#include "core/text_list.h"
#include "powerups.h"
#include "effects.h"

Player players[MAX_PLAYERS] = {0};
Player* player = &players[0];
Player* player2 = NULL;

char* player_names[MAX_PLAYERS+1]; // used for name dropdown. +1 for ALL option.

int player_image = -1;



void player_set_controls()
{
    window_controls_clear_keys();

    window_controls_add_key(&player->actions[PLAYER_ACTION_FORWARD].state, GLFW_KEY_W);
    window_controls_add_key(&player->actions[PLAYER_ACTION_BACKWARD].state, GLFW_KEY_S);
    window_controls_add_key(&player->actions[PLAYER_ACTION_LEFT].state, GLFW_KEY_A);
    window_controls_add_key(&player->actions[PLAYER_ACTION_RIGHT].state, GLFW_KEY_D);
    window_controls_add_key(&player->actions[PLAYER_ACTION_SCUM].state, GLFW_KEY_LEFT_SHIFT);
    window_controls_add_key(&player->actions[PLAYER_ACTION_SHOOT].state, GLFW_KEY_SPACE);
    window_controls_add_key(&player->actions[PLAYER_ACTION_SHIELD].state, GLFW_KEY_F);
    window_controls_add_key(&player->actions[PLAYER_ACTION_RESET].state, GLFW_KEY_R);
    window_controls_add_key(&player->actions[PLAYER_ACTION_PAUSE].state, GLFW_KEY_P);

    if(player2 != NULL && role == ROLE_LOCAL)
    {
        window_controls_add_key(&player2->actions[PLAYER_ACTION_FORWARD].state, GLFW_KEY_UP);
        window_controls_add_key(&player2->actions[PLAYER_ACTION_BACKWARD].state, GLFW_KEY_DOWN);
        window_controls_add_key(&player2->actions[PLAYER_ACTION_LEFT].state, GLFW_KEY_LEFT);
        window_controls_add_key(&player2->actions[PLAYER_ACTION_RIGHT].state, GLFW_KEY_RIGHT);
        window_controls_add_key(&player2->actions[PLAYER_ACTION_SHOOT].state, GLFW_KEY_RIGHT_SHIFT);
    }
}

void players_init()
{
    if(player_image != -1)
        return;

    player_image = gfx_load_image("src/img/spaceship.png", false, false, 32, 32);

    GFXImage* img = &gfx_images[player_image];
    float wh = MAX(img->element_width, img->element_height)*0.9;

    for(int i = 0; i < MAX_PLAYERS; ++i)
    {
        Player* p = &players[i];
        memset(p,0, sizeof(Player));

        p->dead = false;
        p->active = false;
        p->deaths = 0;
        p->id = i;

        if(p == player)
        {
            p->pos.x = view_width/2.0;
            p->pos.y = view_height/2.0;
        }
        else
        {   p->settings.color = COLOR_RAND2;
            p->pos.x = rand()%view_width;
            p->pos.y = rand()%view_height;
        }

        p->vel.x = 0.0;
        p->vel.y = 0.0;
        p->angle_deg = 90.0;
        p->accel_factor = 16.0;
        p->turn_rate = 7.0;
        p->velocity_limit = 500.0;
        p->energy = MAX_ENERGY/2.0;
        p->force_field = false;
        p->invincible = false;
        p->hp_max = 100.0;
        p->hp = p->hp_max;

        p->hit_box.x = p->pos.x;
        p->hit_box.y = p->pos.y;
        p->hit_box.w = wh;
        p->hit_box.h = wh;

        memset(p->settings.name, PLAYER_NAME_MAX, 0);
        sprintf(p->settings.name, "Player %d", i);

        memcpy(&p->hit_box_prior, &p->hit_box, sizeof(Rect));

        if(role != ROLE_SERVER)
        {
            ParticleSpawner* j = particles_spawn_effect(p->pos.x,p->pos.y, 0, &particle_effects[EFFECT_JETS],0.0,true,true);
            p->jets_id = j->id;
        }
    }

}

Player* player_get_by_id(uint8_t id)
{
    for(int i = 0; i < MAX_PLAYERS; ++i)
    {
        if(players[i].id == id)
            return &players[i];
    }
    return NULL;
}

void player_set_active_state(uint8_t id, bool active)
{
    Player* p = player_get_by_id(id);
    if(p == NULL) return;

    p->active = active;

    ParticleSpawner* jets = get_spawner_by_id(p->jets_id);
    if(jets) jets->hidden = !active;
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

    if(p->dead) return;

    memcpy(&p->hit_box_prior, &p->hit_box, sizeof(Rect));

    bool fwd   = p->actions[PLAYER_ACTION_FORWARD].state;
    bool bkwd  = p->actions[PLAYER_ACTION_BACKWARD].state;
    bool left  = p->actions[PLAYER_ACTION_LEFT].state;
    bool right = p->actions[PLAYER_ACTION_RIGHT].state;
    bool scum = p->actions[PLAYER_ACTION_SCUM].state;

    if(p->ai)
    {
        int count = num_players-1;
        if(count > 0)
        {

            Player* target_player = NULL;
            float min_d = view_width*view_height;

            for(int i = 0; i < MAX_PLAYERS; ++i)
            {
                if(!players[i].active) continue;
                if(players[i].dead) continue;
                if(players[i].id == p->id) continue;
                if(!can_target_player && &players[i] == player) continue;

                if(p == player2 && &players[i] == player && num_players > 2) continue; //player 2 won't target player if more than 2 players

                float d = dist(p->pos.x, p->pos.y, players[i].pos.x, players[i].pos.y);
                if(d < min_d)
                {
                    target_player = &players[i];
                    min_d = d;
                }
            }

            if(target_player != NULL)
            {
                float target_angle = calc_angle_deg(p->pos.x, p->pos.y, target_player->pos.x, target_player->pos.y);

                float dif = calc_angle_dif(p->angle_deg, target_angle);
                float adif = ABS(dif);

                if(adif > 0.5 && rand()%11<=9)
                {

                    float adj = 1.0 * dif/adif;

                    adj *= p->turn_rate;
                    if(adif < ABS(adj))
                    {
                        adj /= 10.0;
                    }

                    // text_list_add(text_lst, 2.0, "%.2f, %.2f, %+.0f  (%.2f)",p->angle_deg, target_angle, adj, dif);
                    p->angle_deg += (adj*2);
                    p->angle_deg = normalize_angle_deg(p->angle_deg);

                    p->actions[PLAYER_ACTION_SHOOT].state = false;
                }

#if 1
                if(adif < 3.0)
                {
                    if(min_d < 300)
                    {
                        if(rand()%6 == 0)
                            p->actions[PLAYER_ACTION_SHOOT].state = true;
                    }
                }
                else
                    p->actions[PLAYER_ACTION_SHOOT].state = false;


                if(rand()%3 == 0)
                    fwd = true;
#endif
                // if(isnan(p->angle_deg))
                // {
                //     printf("%s angle was nan!\n", p->settings.name);
                //     p->angle_deg = 0.0;
                // }

            }

            // if no targets then they're the winner

        }

    }

    float acc_factor_adj = 1.0;
    float turn_factor_adj = 1.0;
    if(scum)
    {
        acc_factor_adj = 0.1;
        turn_factor_adj = 0.1;
    }

    // apply "space friction"
    Vector2f uv = {p->vel.x, p->vel.y};
    float m = magn(p->vel);
    normalize(&uv);
    p->vel.x -= 0.05*m*uv.x;
    p->vel.y -= 0.05*m*uv.y;

    float angle = RAD(p->angle_deg);

    if(fwd || bkwd)
    {

        if(fwd)
        {
            p->vel.x += p->accel_factor*acc_factor_adj*cos(angle);
            p->vel.y -= p->accel_factor*acc_factor_adj*sin(angle);
        }
        if(bkwd)
        {
            p->vel.x -= p->accel_factor*acc_factor_adj*cos(angle);
            p->vel.y += p->accel_factor*acc_factor_adj*sin(angle);
        }

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

    // set velocity to zero if it is close enough
    if(ABS(p->vel.x) < 0.1) p->vel.x = 0.000;
    if(ABS(p->vel.y) < 0.1) p->vel.y = 0.000;

    if((p == player || (p == player2 && !p->ai)) && easy_movement && fwd)
    {
        p->pos.x += delta_t*300.0*cos(angle);
        p->pos.y -= delta_t*300.0*sin(angle);
        p->vel.x = 0;
        p->vel.y = 0;
    }
    else
    {
        p->pos.x += p->vel.x*delta_t;
        p->pos.y += p->vel.y*delta_t;
    }


    if(left)
    {
        p->angle_deg += p->turn_rate*turn_factor_adj;
        p->angle_deg = normalize_angle_deg(p->angle_deg);
    }

    if(right)
    {
        p->angle_deg -= p->turn_rate*turn_factor_adj;
        p->angle_deg = normalize_angle_deg(p->angle_deg);
    }

    if(p->actions[PLAYER_ACTION_RESET].toggled_on)
    {
        player_reset(p);
    }

    player_update_positions(p);

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

        float xa = cosf(vel_angle);
        float ya = sinf(vel_angle);

        float new_angle = 0;
        if(!FEQ0(adj.x))
        {
            new_angle = PI - vel_angle;
        }
        else
        {
            new_angle = PI*2 - vel_angle;
        }

        float vel_adj = 0.8;
        if(!FEQ0(xa))
        {
            float xcomp = p->vel.x / xa;
            p->vel.x = xcomp*cos(new_angle)*vel_adj;
        }
        if(!FEQ0(ya))
        {
            float ycomp = p->vel.y / ya;
            p->vel.y = ycomp*sin(new_angle)*vel_adj;
        }

    }

    // memcpy(&p->hit_box_prior, &p->hit_box, sizeof(Rect));
    player_update_positions(p);

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
    const float p_energy = 0.0;
    if(p->actions[PLAYER_ACTION_SHOOT].state)
    {
        p->proj_cooldown -= delta_t;
        if(p->proj_cooldown <= 0.0)
        {
            projectile_add(p, 0, p_energy);
            p->proj_cooldown = pcooldown;
        }
    }

    // check collisions with powerups
    int num_powerups = powerups_get_count();
    Powerup* powerups = powerups_get_list();

    for(int i = 0; i < num_powerups; ++i)
    {
        Powerup* pup = &powerups[i];
        if(pup->picked_up)
            continue;

        // simple check
        bool c1 = rectangles_colliding(&p->hit_box, &pup->hit_box);
        bool c2 = false;
        if(!c1)
        {
            // complex check
            c2 = are_rects_colliding(&p->hit_box_prior, &p->hit_box, &pup->hit_box);
        }

        if(c1 || c2)
        {
            // picked up powerup!
            pup->picked_up = true;
            pup->picked_up_player = p;
            pup->func(p, false);

            if(role == ROLE_SERVER)
            {
                switch(pup->type)
                {
                    case POWERUP_TYPE_INVINCIBILITY:
                        server_send_event(EVENT_TYPE_HOLY, pup->pos.x, pup->pos.y);
                        break;
                    case POWERUP_TYPE_HEALTH:
                        server_send_event(EVENT_TYPE_HEAL, pup->pos.x, pup->pos.y);
                        break;
                    case POWERUP_TYPE_HEALTH_FULL:
                        server_send_event(EVENT_TYPE_HEAL_FULL, pup->pos.x, pup->pos.y);
                        break;
                    default:
                        break;
                }
                continue;
            }

            switch(pup->type)
            {
                case POWERUP_TYPE_INVINCIBILITY:
                    particles_spawn_effect(pup->pos.x, pup->pos.y, 1, &particle_effects[EFFECT_HOLY1], 1.0, true, false);
                    break;
                case POWERUP_TYPE_HEALTH:
                    particles_spawn_effect(p->pos.x, p->pos.y, 1, &particle_effects[EFFECT_HEAL1], 1.0, true, false);
                    break;
                case POWERUP_TYPE_HEALTH_FULL:
                    particles_spawn_effect(p->pos.x, p->pos.y, 1, &particle_effects[EFFECT_HEAL2], 1.0, true, false);
                    break;
                default:
                    break;
            }
        }
    }

    // TEST
    // player_hurt(p,0.1);

    float energy = 16.0*delta_t;
    player_add_energy(p, energy);

    // printf("energy: %.2f\n", p->energy);
}

void player_add_energy(Player* p, float e)
{
    p->energy = RANGE(p->energy + e, 0.0, MAX_ENERGY);
}

void player_die(Player* p)
{
    p->deaths += 1;
    if(p->deaths >= game_settings.num_lives)
    {
        p->dead = true;
        ParticleSpawner* jets = get_spawner_by_id(p->jets_id);
        if(jets) jets->hidden = true;
        // else printf("warning: jets is NULL\n");
        printf("%s is dead!\n", p->settings.name);
        text_list_add(text_lst, 4.0, "%s is dead", p->settings.name);
        server_send_message(TO_ALL, FROM_SERVER, "%s is dead", p->settings.name);
    }
    else
    {
        player_respawn(p);
        text_list_add(text_lst, 3.0, "%s died", p->settings.name);
        server_send_message(TO_ALL, FROM_SERVER, "%s died", p->settings.name);
    }

    player_determine_winner();

}

void player_determine_winner()
{
    int num_dead = 0;
    for(int i = 0; i < MAX_PLAYERS; ++i)
    {
        Player* pl = &players[i];
        if(!pl->active) continue;
        if(pl->dead)
        {
            num_dead++;
        }
        else
        {
            winner_index = i;
        }
    }

    // GAME OVER
    if(num_dead == (num_players-1))
    {
        screen = SCREEN_GAME_END;
    }
}

void player_hurt(Player* p, float damage)
{
    if(!p->active) return;
    if(p->dead) return;
    if(p->invincible) return;
    if(game_status == GAME_STATUS_LIMBO) return;

    p->hp -= damage;
    if(p->hp <= 0)
    {
        p->hp = 0;
        player_die(p);
    }
}

void player_heal(Player* p, float hp)
{
    if(!p->active) return;
    if(p->dead) return;

    p->hp += hp;
    if(p->hp > p->hp_max)
    {
        p->hp = p->hp_max;
    }
}

void player_draw(Player* p)
{
    if(!p->active) return;
    if(p->dead) return;

    GFXImage* img = &gfx_images[player_image];
    if(p->settings.sprite_index >= img->element_count)
    {
        p->settings.sprite_index = 0;
    }

    if(p->invincible)
    {
        gfx_draw_image(player_image, p->settings.sprite_index, p->pos.x, p->pos.y, 0x00FFD700, 1.0, p->angle_deg, 1.0, false, true);
    }
    else
    {
        gfx_draw_image_color_mask(player_image, p->settings.sprite_index, p->pos.x, p->pos.y, p->settings.color, 1.0, p->angle_deg, 1.0, false, true);
    }

    if(p->force_field)
    {
        gfx_draw_circle(p->pos.x, p->pos.y, p->hit_box.w+3, COLOR_BLUE, 0.2, true, true);
    }

    float name_scale = 0.15;
    float name_x = p->pos.x - p->hit_box.w/2.0;
    float name_y = p->pos.y + p->hit_box.h/2.0 + 5;
    Vector2f title_size = gfx_string_get_size(name_scale, p->settings.name);
    gfx_draw_string(name_x, name_y, p->settings.color, name_scale, 0.0, 0.5, true, true, p->settings.name);

    if(game_debug_enabled)
    {
        gfx_draw_string(name_x, name_y+title_size.y, p->settings.color, name_scale, 0.0, 0.5, true, true, "%d", p->deaths);

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

        x1 = x0 + 100*cosf(RAD(p->angle_deg));
        y1 = y0 - 100*sinf(RAD(p->angle_deg));
        gfx_add_line(x0, y0, x1, y1, COLOR_CYAN);

        // if(p == player)
        // {
        //     Vector2f l = gfx_draw_string(10.0, view_height-30.0, COLOR_BLACK, 0.15, 0.0, 1.0, true, false, "%8.2f, %8.2f", p->vel.x, p->vel.y);
        //     gfx_draw_string(10.0, view_height-30.0+l.y, COLOR_BLACK, 0.15, 0.0, 1.0, true, false, "%8.2f, %8.2f", p->pos.x, p->pos.y);
        // }
    }

    if(game_status == GAME_STATUS_RUNNING)
    {
        if(p == player)
        {

            // draw hp
            float hp_full_w  = view_width/2.0;
            float hp_w = hp_full_w*(p->hp/p->hp_max);
            float hp_h = 15.0;
            float hp_x = (view_width - hp_full_w)/2.0;
            float hp_y = view_height - hp_h - 20.0;
            gfx_draw_rect_xywh_tl(hp_x-1, hp_y-1, hp_full_w+2, hp_h+2, COLOR_BLACK, 0.0,1.0,0.7, true, true);
            gfx_draw_rect_xywh_tl(hp_x, hp_y, hp_w, hp_h, 0x00CC0000, 0.0,1.0,0.4, true, false);

            // draw energy
            float energy_width = hp_full_w*(p->energy/MAX_ENERGY);
            float pad = 3.0;
            float energy_h = 4.0;
            float energy_y = hp_y + hp_h + pad;
            gfx_draw_rect_xywh_tl(hp_x-1, energy_y-1, hp_full_w+2, energy_h+2, COLOR_BLACK, 0.0,1.0,0.7, true, false);
            gfx_draw_rect_xywh_tl(hp_x, energy_y, energy_width, energy_h, COLOR_YELLOW, 0.0,1.0,0.4, true, false);
        }
        else
        {
            float w = img->element_width;
            float h = img->element_height;
            float hp_w = w * (p->hp/p->hp_max);
            float hp_x = p->pos.x - w/2.0;
            float hp_y = p->pos.y + h/2.0 + 2.0;
            gfx_draw_rect_xywh_tl(hp_x, hp_y, hp_w, 2, 0x00CC0000, 0.0,1.0,0.4, true, true);
        }
    }
}

void player_update_positions(Player* p)
{
    p->hit_box.x = p->pos.x;
    p->hit_box.y = p->pos.y;

    if(role != ROLE_SERVER)
    {
        ParticleSpawner* jets = get_spawner_by_id(p->jets_id);
        if(jets)
        {
            Rect* r = &gfx_images[player_image].visible_rects[p->settings.sprite_index];
            jets->pos.x = p->pos.x - 0.5*r->w*cosf(RAD(p->angle_deg));
            jets->pos.y = p->pos.y + 0.5*r->w*sinf(RAD(p->angle_deg));
        }
    }
}

void player_respawn(Player* p)
{
    //TODO: position
    p->pos.x = rand()%view_width;
    p->pos.y = rand()%view_height;
    p->vel.x = 0.0;
    p->vel.y = 0.0;
    p->energy = MAX_ENERGY;
    p->hp = p->hp_max;
}

void player_reset(Player* p)
{
    p->pos.x = VIEW_WIDTH/2.0;
    p->pos.y = VIEW_HEIGHT/2.0;
    p->vel.x = 0.0;
    p->vel.y = 0.0;
    p->energy = MAX_ENERGY;
    p->hp = p->hp_max;
    p->dead = false;
    p->deaths = 0;

    memset(p->actions,0, sizeof(p->actions));
}

// should be equal to num_players
int players_get_num_active()
{
    int count = 0;
    for(int i = 0; i < MAX_PLAYERS; ++i)
    {
        Player* p = &players[i];
        if(p->active) count++;
    }
    return count;
}

int players_get_num_ai()
{
    int count = 0;
    for(int i = 0; i < MAX_PLAYERS; ++i)
    {
        Player* p = &players[i];
        if(p->active && p->ai) count++;
    }
    return count;
}


int player_names_build(bool include_all, bool only_active)
{
    int count = 0;

    // fill out useful player_names array
    for(int i = 0; i < MAX_PLAYERS; ++i)
    {
        Player* p = &players[i];
        if(only_active && !p->active) continue;

        if(player_names[count+1]) free(player_names[count+1]);

        int namelen  = strlen(p->settings.name);
        player_names[count+1] = calloc(namelen+1, sizeof(char));
        strncpy(player_names[count+1], p->settings.name, namelen);
        count++;
    }

    if(include_all)
    {
        if(player_names[0]) free(player_names[0]);
        player_names[0] = calloc(4, sizeof(char));
        strncpy(player_names[0],"ALL",3);
        count++;
    }

    return count;
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

    // printf("[lerp prior]  %.2f, %.2f\n", p->server_state_prior.pos.x, p->server_state_prior.pos.y);
    // printf("[lerp target] %.2f, %.2f\n", p->server_state_target.pos.x, p->server_state_target.pos.y);

    Vector2f lp = lerp2f(&p->server_state_prior.pos,&p->server_state_target.pos,t);
    p->pos.x = lp.x;
    p->pos.y = lp.y;

    p->angle_deg = lerp_angle_deg(p->server_state_prior.angle, p->server_state_target.angle, t);

    p->energy = lerp(p->server_state_prior.energy, p->server_state_target.energy,t);
    p->hp = lerp(p->server_state_prior.hp, p->server_state_target.hp, t);

}
