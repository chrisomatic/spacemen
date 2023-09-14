#include "headers.h"

#include "core/imgui.h"
#include "core/particles.h"
#include "core/io.h"

#include "window.h"
#include "gfx.h"
#include "log.h"
#include "player.h"
#include "projectile.h"
#include "effects.h"
#include "editor.h"

static ParticleSpawner* particle_spawner; 
static char particles_file_name[20] = {0};

static void randomize_effect(ParticleEffect* effect);

void editor_init()
{
    // create particle effect used for editor
    ParticleEffect effect = {
        .life = {3.0,5.0,1.0},
        .scale = {0.2,0.5,-0.05},
        .velocity_x = {-16.0,16.0,0.0},
        .velocity_y = {-16.0,16.0,0.0},
        .opacity = {0.6,1.0,-0.2},
        .angular_vel = {0.0,0.0,0.0},
        .rotation_init_min = 0.0,
        .rotation_init_max = 0.0,
        .color1 = 0x00FF00FF,
        .color2 = 0x00CC0000,
        .color3 = 0x00202020,
        .spawn_radius_min = 0.0,
        .spawn_radius_max = 1.0,
        .spawn_time_min = 0.2,
        .spawn_time_max = 0.5,
        .burst_count_min = 1,
        .burst_count_max = 3,
        .sprite_index = 57,
        .img_index = particles_image,
        .use_sprite = true,
        .blend_additive = false,
    };

    randomize_effect(&effect);

    particle_spawner = particles_spawn_effect(200, 120, &effect, 0, false, true);
}

void editor_draw()
{
    imgui_begin_panel("Editor", 10, 10, true);

        imgui_newline();
        char* buttons[] = {"Players", "Projectiles", "Particles", "Console"};
        int selection = imgui_button_select(IM_ARRAYSIZE(buttons), buttons, "");

        switch(selection)
        {
            case 0: // players
            {
                imgui_color_picker("Color", &player->settings.color);
            } break;
            case 1: // projectiles
            {
                imgui_slider_float("Damage", 1.0,100.0,&projectile_lookup[0].damage);
                imgui_slider_float("Base Speed", 200.0,1000.0,&projectile_lookup[0].base_speed);
                imgui_slider_float("Min Speed", 50.0,200.0,&projectile_lookup[0].min_speed);
            } break;
            case 2: // particles
            {
                ParticleEffect* effect = &particle_spawner->effect;
                particle_spawner->hidden = false;

                int big = 12;
                imgui_set_text_size(10);

                imgui_horizontal_begin();
                if(imgui_button("Randomize##particle_spawner"))
                {
                    randomize_effect(effect);
                }
                if(imgui_button("Reload Effects##particle_spawner"))
                {
                    effects_load_all();
                }

                imgui_horizontal_end();

                //imgui_set_slider_width(60);
                imgui_text_sized(8,"Particle Count: %d",particle_spawner->particle_list->count);
                imgui_text_sized(big,"Particle Life");
                imgui_horizontal_begin();
                    imgui_slider_float("Min##life", 0.1,5.0,&effect->life.init_min);
                    imgui_slider_float("Max##life", 0.1,5.0,&effect->life.init_max);
                    effect->life.init_max = (effect->life.init_min > effect->life.init_max ? effect->life.init_min : effect->life.init_max);
                    imgui_slider_float("Rate##life", 0.1,5.0,&effect->life.rate);
                imgui_horizontal_end();
                imgui_text_sized(big,"Rotation");
                imgui_horizontal_begin();
                    imgui_slider_float("Min##rotation", -360.0,360.0,&effect->rotation_init_min);
                    imgui_slider_float("Max##rotation", -360.0,360.0,&effect->rotation_init_max);
                    effect->rotation_init_max = (effect->rotation_init_min > effect->rotation_init_max ? effect->rotation_init_min : effect->rotation_init_max);
                imgui_horizontal_end();
                imgui_text_sized(big,"Scale");
                imgui_horizontal_begin();
                    imgui_slider_float("Min##scale", 0.01,3.00,&effect->scale.init_min);
                    imgui_slider_float("Max##scale", 0.01,3.00,&effect->scale.init_max);
                    effect->scale.init_max = (effect->scale.init_min > effect->scale.init_max ? effect->scale.init_min : effect->scale.init_max);
                    imgui_slider_float("Rate##scale", -1.0,1.0,&effect->scale.rate);
                imgui_horizontal_end();
                imgui_text_sized(big,"Velocity X");
                imgui_horizontal_begin();
                    imgui_slider_float("Min##velx", -300.0,300.0,&effect->velocity_x.init_min);
                    imgui_slider_float("Max##velx", -300.0,300.0,&effect->velocity_x.init_max);
                    effect->velocity_x.init_max = (effect->velocity_x.init_min > effect->velocity_x.init_max ? effect->velocity_x.init_min : effect->velocity_x.init_max);
                    imgui_slider_float("Rate##velx", -300.0,300.0,&effect->velocity_x.rate);
                imgui_horizontal_end();
                imgui_text_sized(big,"Velocity Y");
                imgui_horizontal_begin();
                    imgui_slider_float("Min##vely", -300.0,300.0,&effect->velocity_y.init_min);
                    imgui_slider_float("Max##vely", -300.0,300.0,&effect->velocity_y.init_max);
                    effect->velocity_y.init_max = (effect->velocity_y.init_min > effect->velocity_y.init_max ? effect->velocity_y.init_min : effect->velocity_y.init_max);
                    imgui_slider_float("Rate##vely", -300.0,300.0,&effect->velocity_y.rate);
                imgui_horizontal_end();
                imgui_text_sized(big,"Opacity");
                imgui_horizontal_begin();
                    imgui_slider_float("Min##opacity", 0.01,1.0,&effect->opacity.init_min);
                    imgui_slider_float("Max##opacity", 0.01,1.0,&effect->opacity.init_max);
                    effect->opacity.init_max = (effect->opacity.init_min > effect->opacity.init_max ? effect->opacity.init_min : effect->opacity.init_max);
                    imgui_slider_float("Rate##opacity", -1.0,1.0,&effect->opacity.rate);
                imgui_horizontal_end();
                imgui_text_sized(big,"Angular Velocity");
                imgui_horizontal_begin();
                    imgui_slider_float("Min##angular_vel", -360.0,360.0,&effect->angular_vel.init_min);
                    imgui_slider_float("Max##angular_vel", -360.0,360.0,&effect->angular_vel.init_max);
                    effect->angular_vel.init_max = (effect->angular_vel.init_min > effect->angular_vel.init_max ? effect->angular_vel.init_min : effect->angular_vel.init_max);
                    imgui_slider_float("Rate##angular_vel", -360.0,360.0,&effect->angular_vel.rate);
                imgui_horizontal_end();
                imgui_text_sized(big,"Spawn Radius");
                imgui_horizontal_begin();
                    imgui_slider_float("Min##spawn_radius", 0.0,64.0,&effect->spawn_radius_min);
                    imgui_slider_float("Max##spawn_radius", 0.0,64.0,&effect->spawn_radius_max);
                    effect->spawn_radius_max = (effect->spawn_radius_min > effect->spawn_radius_max ? effect->spawn_radius_min : effect->spawn_radius_max);
                imgui_horizontal_end();
                imgui_text_sized(big,"Spawn Time");
                imgui_horizontal_begin();
                    imgui_slider_float("Min##spawn_time", 0.01,2.0,&effect->spawn_time_min);
                    imgui_slider_float("Max##spawn_time", 0.01,2.0,&effect->spawn_time_max);
                    effect->spawn_time_max = (effect->spawn_time_min > effect->spawn_time_max ? effect->spawn_time_min : effect->spawn_time_max);
                imgui_horizontal_end();
                imgui_text_sized(big,"Burst Count");
                imgui_horizontal_begin();
                    imgui_number_box("Min##burst_count", 1, 20, &effect->burst_count_min);
                    imgui_number_box("Max##burst_count", 1, 20, &effect->burst_count_max);
                    effect->burst_count_max = (effect->burst_count_min > effect->burst_count_max ? effect->burst_count_min : effect->burst_count_max);
                imgui_horizontal_end();
                imgui_checkbox("Use Sprite",&effect->use_sprite);
                if(effect->use_sprite)
                {
                    imgui_horizontal_begin();
                    imgui_number_box("Img Index##img_index", 0, MAX_GFX_IMAGES-1, &effect->img_index);
                    imgui_number_box("Sprite Index##sprite_index", 0, MAX_GFX_IMAGES-1, &effect->sprite_index);
                    imgui_horizontal_end();
                }
                imgui_text_sized(big,"Colors");
                imgui_horizontal_begin();
                    imgui_color_picker("1##colors", &effect->color1);
                    imgui_color_picker("2##colors", &effect->color2);
                    imgui_color_picker("3##colors", &effect->color3);
                imgui_horizontal_end();
                imgui_checkbox("Blend Addtive",&effect->blend_additive);
                imgui_newline();

                imgui_horizontal_begin();

                    imgui_text_box("##file_name_particles",particles_file_name,IM_ARRAYSIZE(particles_file_name));

                    char file_path[64]= {0};
                    snprintf(file_path,63,"src/effects/%s.effect",particles_file_name);

                    if(imgui_button("Save##particles"))
                    {
                        effects_save(file_path, effect);
                    }
                    if(imgui_button("Load##particles"))
                    {
                        ParticleEffect loaded_effect = {0};
                        bool res = effects_load(file_path, &loaded_effect);
                        if(res)
                        {
                            memcpy(effect,&loaded_effect,sizeof(ParticleEffect));
                        }
                    }

                imgui_horizontal_end();

                if(io_file_exists(file_path))
                {
                    imgui_text_colored(0x00CC8800, "File Exists!");
                }

                // show preview
                particles_draw_spawner(particle_spawner, true, false);
            }

            case 3: // console
            {

            } break;
        }
    imgui_end();

}

static void randomize_effect(ParticleEffect* effect)
{
    effect->life.init_min = RAND_FLOAT(0.1,5.0);
    effect->life.init_max = RAND_FLOAT(0.1,5.0);
    effect->life.rate = 1.0;

    effect->rotation_init_min = RAND_FLOAT(-360.0,360.0);
    effect->rotation_init_max = RAND_FLOAT(-360.0,360.0);

    effect->scale.init_min = RAND_FLOAT(0.01,1.0);
    effect->scale.init_max = RAND_FLOAT(0.01,1.0);
    effect->scale.rate     = RAND_FLOAT(-0.5,0.5);

    effect->velocity_x.init_min = RAND_FLOAT(-32.0,32.0);
    effect->velocity_x.init_max = RAND_FLOAT(-32.0,32.0);
    effect->velocity_x.rate     = RAND_FLOAT(-32.0,32.0);

    effect->velocity_y.init_min = RAND_FLOAT(-32.0,32.0);
    effect->velocity_y.init_max = RAND_FLOAT(-32.0,32.0);
    effect->velocity_y.rate     = RAND_FLOAT(-32.0,32.0);

    effect->opacity.init_min = RAND_FLOAT(0.01,1.0);
    effect->opacity.init_max = RAND_FLOAT(0.01,1.0);
    effect->opacity.rate     = RAND_FLOAT(-1.0,1.0);

    effect->angular_vel.init_min = RAND_FLOAT(-360.0, 360.0);
    effect->angular_vel.init_max = RAND_FLOAT(-360.0, 360.0);
    effect->angular_vel.rate     = RAND_FLOAT(-360.0, 360.0);

    effect->spawn_radius_min  = RAND_FLOAT(0.0, 64.0);
    effect->spawn_radius_max  = RAND_FLOAT(0.0, 64.0);

    effect->spawn_time_min  = RAND_FLOAT(0.01, 2.0);
    effect->spawn_time_max  = RAND_FLOAT(0.01, 2.0);

    effect->burst_count_min  = RAND_FLOAT(1, 20);
    effect->burst_count_max  = RAND_FLOAT(1, 20);

    effect->img_index = particles_image;
    effect->sprite_index = RAND_RANGE(0,79);

    effect->color1 = RAND_RANGE(0x0,0x00FFFFFF);
    effect->color2 = RAND_RANGE(0x0,0x00FFFFFF);
    effect->color3 = RAND_RANGE(0x0,0x00FFFFFF);

    effect->blend_additive = RAND_RANGE(0,1) == 1 ? true : false;
}