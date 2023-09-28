#include "headers.h"
#include "main.h"
#include "window.h"
#include "shader.h"
#include "timer.h"
#include "gfx.h"
#include "math2d.h"
#include "imgui.h"
#include "log.h"
#include "projectile.h"
#include "player.h"
#include "net.h"
#include "particles.h"
#include "effects.h"
#include "settings.h"
#include "editor.h"
#include "powerups.h"
#include "text_list.h"

/*
TODO:
1) handle disconnection from server

*/

// =========================
// Global Vars
// =========================

text_list_t* text_lst = NULL;

DisplayScreen screen = SCREEN_HOME;
GameStatus game_status = GAME_STATUS_LIMBO;
bool initialized = false;
bool back_to_home = false;
bool paused = false;
bool debug_enabled = false;
bool game_debug_enabled = false;
bool initiate_game = false;
int num_lives = 1;
int num_players = 2;
float game_end_counter;
int winner_index = 0;
int client_id = -1;

ParticleEffect mouse_click_effect = {0};
// ParticleSpawner* mouse_click_spawner = NULL;


// local game vars
bool can_target_player = false;
bool all_active = false;
bool all_ai = false;


// mouse
int mx=0, my=0;

Timer game_timer = {0};
GameRole role;
Rect world_box = {0};
Rect ready_zone;

// Settings
uint32_t background_color = 0x00303030;
int menu_selected_option = 0;

Settings menu_settings = {0};

#define NUM_STARS   1000
Vector2i stars[NUM_STARS] = {0};
int stars_size[NUM_STARS] = {0};
int stars_image = -1;

typedef struct
{
    bool up;
    bool down;
    bool enter;
} MenuKeys;
MenuKeys menu_keys_prior = {0};
MenuKeys menu_keys = {0};

typedef void (*loop_update_func)(float _dt, bool is_client);
typedef void (*loop_draw_func)(bool is_client);


// =========================
// Function Prototypes
// =========================

void parse_args(int argc, char* argv[]);

void init();
void init_server();
void deinit();

void reset_game();

void run_loop(DisplayScreen _screen, loop_update_func _update, loop_draw_func _draw);

void run_home();
void update_home(float _dt, bool is_client);
void draw_home(bool is_client);

void run_settings();
void update_settings(float _dt, bool is_client);
void draw_settings(bool is_client);

void run_game_start();
void update_game_start(float _dt, bool is_client);
void draw_game_start(bool is_client);

void run_game_end();
void update_game_end(float _dt, bool is_client);
void draw_game_end(bool is_client);

void simulate(double);
void simulate_client(double);
void run_game();
void update_game(float _dt, bool is_client);
void draw_game(bool is_client);

void run_server();

void stars_init();
void stars_update();
void stars_draw();
void player_list_draw();

void key_cb(GLFWwindow* window, int key, int scan_code, int action, int mods);

// =========================
// Main Loop
// =========================

int main(int argc, char* argv[])
{

    // test_packing();
    // exit(0);

    init_timer();
    log_init(0);

    screen = SCREEN_HOME;
    parse_args(argc, argv);

    time_t t;
    srand((unsigned) time(&t));

    if(role != ROLE_SERVER)
        init();

    text_lst = text_list_init(50, 10.0, view_height - 10.0, 0.12, COLOR_WHITE, false, TEXT_ALIGN_LEFT);
    // text_lst = text_list_init(100, view_width-10.0, view_height-10.0, 0.1, COLOR_WHITE, false, TEXT_ALIGN_RIGHT);

    for(;;)
    {

        if(back_to_home)
        {
            back_to_home = false;
            role = ROLE_UNKNOWN;
            screen = SCREEN_HOME;
        }

        switch(screen)
        {
            case SCREEN_HOME:
                run_home();
                break;
            case SCREEN_SETTINGS:
                run_settings();
                break;
            case SCREEN_SERVER:
                run_server();
                break;
            case SCREEN_GAME_START:
                run_game_start();
                break;
            case SCREEN_GAME:
                run_game();
                break;
            case SCREEN_GAME_END:
                run_game_end();
                break;
        }

        if(window_should_close())
        {
            net_client_disconnect();
            break;
        }
    }

    return 0;
}

void parse_args(int argc, char* argv[])
{
    role = ROLE_UNKNOWN;

    if(argc > 1)
    {
        for(int i = 1; i < argc; ++i)
        {
            if(argv[i][0] == '-' && argv[i][1] == '-')
            {
                // local
                if(strncmp(argv[i]+2,"local",5) == 0)
                {
                    role = ROLE_LOCAL;
                    screen = SCREEN_GAME_START;
                }

                // server
                else if(strncmp(argv[i]+2,"server",6) == 0)
                {
                    role = ROLE_SERVER;
                    screen = SCREEN_SERVER;
                }


                // client
                else if(strncmp(argv[i]+2,"client",6) == 0)
                {
                    role = ROLE_CLIENT;
                    screen = SCREEN_GAME_START;
                }
            }
            else
            {
                net_client_set_server_ip(argv[i]);
            }
        }
    }
}

void init()
{
    if(initialized) return;
    initialized = true;

    LOGI("Resolution: %d %d",VIEW_WIDTH, VIEW_HEIGHT);
    bool success = window_init(VIEW_WIDTH, VIEW_HEIGHT);

    if(!success)
    {
        fprintf(stderr,"Failed to initialize window!\n");
        exit(1);
    }

    window_controls_set_cb(key_cb);
    window_controls_set_key_mode(KEY_MODE_NORMAL);

    LOGI("Initializing...");

    LOGI(" - Shaders.");
    shader_load_all();

    LOGI(" - Graphics.");
    gfx_init(VIEW_WIDTH, VIEW_HEIGHT);
    world_box.w = view_width;
    world_box.h = view_height;
    world_box.x = view_width/2.0;
    world_box.y = view_height/2.0;

    ready_zone.w = 100;
    ready_zone.h = 100;
    ready_zone.x = view_width-200;
    ready_zone.y = view_height-200;

    LOGI(" - Effects.");
    effects_load_all();

    LOGI(" - Particles.");
    particles_init();

    LOGI(" - Players.");
    players_init();

    LOGI(" - Projectile.");
    projectile_init();

    LOGI(" - Powerups.");
    powerups_init();

    LOGI(" - Editor.");
    editor_init();


    LOGI(" - Settings.");
    settings_load();
    memcpy(&player->settings, &menu_settings, sizeof(Settings));

    imgui_load_theme("retro.theme");

    stars_init();

    memcpy(&mouse_click_effect, &particle_effects[EFFECT_PLAYER_ACTIVATE],sizeof(ParticleEffect));
    // mouse_click_spawner = particles_spawn_effect(0,0, &mouse_click_effect,1.0,true,true);
}


void init_server()
{
    view_width = VIEW_WIDTH;
    view_height = VIEW_HEIGHT;

    world_box.w = view_width;
    world_box.h = view_height;
    world_box.x = view_width/2.0;
    world_box.y = view_height/2.0;

    ready_zone.w = 100;
    ready_zone.h = 100;
    ready_zone.x = view_width-200;
    ready_zone.y = view_height-200;

    gfx_image_init();
    players_init();
    projectile_init();
}

void deinit()
{
    if(!initialized) return;
    initialized = false;
    shader_deinit();
    window_deinit();
}

void reset_game()
{
    projectile_clear_all();
    players_init();
}

void run_loop(DisplayScreen _screen, loop_update_func _update, loop_draw_func _draw)
{
    bool is_client = (role == ROLE_CLIENT);

    timer_set_fps(&game_timer,TARGET_FPS);
    timer_begin(&game_timer);
    double curr_time = timer_get_time();
    double new_time  = 0.0;
    double accum = 0.0;
    const double dt = 1.0/TARGET_FPS;

    back_to_home = false;

    // loop
    for(;;)
    {
        new_time = timer_get_time();
        double frame_time = new_time - curr_time;
        curr_time = new_time;
        accum += frame_time;

        window_poll_events();
        if(window_should_close())
            break;

        if(back_to_home)
        {
            net_client_disconnect();
            break;
        }

        if(screen != _screen)
            break;

        while(accum >= dt)
        {
            if(_update != NULL) _update(dt, is_client);
            accum -= dt;
        }

        if (_draw != NULL) {
            _draw(is_client);
        }

        timer_wait_for_frame(&game_timer);

        window_swap_buffers();
        window_mouse_update_actions();
    }
}

void run_home()
{
    window_controls_clear_keys();
    window_controls_add_key(&menu_keys.up,    GLFW_KEY_W);
    window_controls_add_key(&menu_keys.down,  GLFW_KEY_S);
    window_controls_add_key(&menu_keys.enter, GLFW_KEY_ENTER);


    run_loop(SCREEN_HOME, update_home, draw_home);
}

void update_home(float _dt, bool is_client)
{
    stars_update();
    text_list_update(text_lst, _dt);

    // if(rand()%1 == 0)
    // {
    //     text_list_add(text_lst, 3.0, "test %d", rand()%30);
    // }
}

void draw_home(bool is_client)
{
    uint8_t r = background_color >> 16;
    uint8_t g = background_color >> 8;
    uint8_t b = background_color >> 0;

    gfx_clear_buffer(r,g,b);

    stars_draw();

    if(menu_keys.down)
    {
        menu_keys.down = false;
        menu_selected_option++;
        if(menu_selected_option > 5) menu_selected_option = 0;
    }

    if(menu_keys.up)
    {
        menu_keys.up = false;
        menu_selected_option--;
        if(menu_selected_option < 0) menu_selected_option = 5;
    }

    if(menu_keys.enter)
    {
        menu_keys.enter = false;
        switch(menu_selected_option)
        {
            case 0:
                role = ROLE_LOCAL;
                screen = SCREEN_GAME_START;
                return;
            case 1:
                net_client_set_server_ip("66.228.36.123");
                role = ROLE_CLIENT;
                screen = SCREEN_GAME_START;
                return;
            case 2:
                role = ROLE_SERVER;
                screen = SCREEN_SERVER;
                return;
            case 3:
                net_client_set_server_ip("127.0.0.1");
                role = ROLE_CLIENT;
                screen = SCREEN_GAME_START;
                return;
            case 4:
                screen = SCREEN_SETTINGS;
                return;
            case 5:
                exit(0);
            default:
                break;
        }
    }

    memcpy(&menu_keys_prior, &menu_keys, sizeof(MenuKeys));

    int num_steps = 80;
    static int ci = -1;
    static uint32_t colors[160] = {0};

    if(ci == -1)
    {
        uint32_t color_list[2] = {COLOR_WHITE, COLOR_BLUE};
        gfx_color_gradient(color_list, 2, num_steps, colors);
        ci = 0;
    }

    // uint8_t tr = (rand() % 256) * (rand()%2);
    // uint8_t tg = (rand() % 256) * (rand()%2);
    // uint8_t tb = (rand() % 256) * (rand()%2);
    float title_scale = 3.0;
    Vector2f title_size = gfx_string_get_size(title_scale, "SPACEMEN");
    gfx_draw_string((view_width-title_size.x)/2.0, (view_height-title_size.y)/4.0, colors[ci], title_scale, 0.0, 1.0, true, true, "SPACEMEN");

    ci++;
    if(ci >= num_steps*2)
        ci = 0;

    int x = (view_width-200)/2.0;
    int y = (view_height+100)/2.0;

    float menu_item_scale = 0.4;

    gfx_draw_string(x,y, menu_selected_option == 0 ? COLOR_YELLOW : COLOR_WHITE, menu_item_scale, 0.0, 1.0, true, false, "Play Local"); y += 32;
    gfx_draw_string(x,y, menu_selected_option == 1 ? COLOR_YELLOW : COLOR_WHITE, menu_item_scale, 0.0, 1.0, true, false, "Play Online"); y += 32;
    gfx_draw_string(x,y, menu_selected_option == 2 ? COLOR_YELLOW : COLOR_WHITE, menu_item_scale, 0.0, 1.0, true, false, "Host Local Server"); y += 32;
    gfx_draw_string(x,y, menu_selected_option == 3 ? COLOR_YELLOW : COLOR_WHITE, menu_item_scale, 0.0, 1.0, true, false, "Join Local Server"); y += 32;
    gfx_draw_string(x,y, menu_selected_option == 4 ? COLOR_YELLOW : COLOR_WHITE, menu_item_scale, 0.0, 1.0, true, false, "Settings"); y += 32;
    gfx_draw_string(x,y, menu_selected_option == 5 ? COLOR_YELLOW : COLOR_WHITE, menu_item_scale, 0.0, 1.0, true, false, "Exit"); y += 32;

    float sel_x = x - 34;
    float sel_y = (view_height+100)/2.0 + (32*menu_selected_option) + 16;

    GFXImage* img = &gfx_images[player_image];
    if(menu_settings.sprite_index >= img->element_count)
    {
        menu_settings.sprite_index = 0;
    }

    gfx_draw_image_color_mask(player_image, menu_settings.sprite_index, sel_x, sel_y, menu_settings.color, 1.0, 0.0, 1.0, true, true);

    text_list_draw(text_lst);

#if 0
    {
        // gfx_draw_string(view_width/2.0, 100, COLOR_BLACK, 0.4, 0.0, 1.0, true, false, "%.2f, %.2f", sel_x, sel_y);

        // GFXImage* img = &gfx_images[player_image];
        int sprite = 0;
        float x = 100;
        float y = 498;
        float vw = img->visible_rects[sprite].w;
        float vh = img->visible_rects[sprite].h;
        printf("[0]\n");
        gfx_draw_image(player_image, sprite, x,y, COLOR_TINT_NONE, 1.0, 0.0, 1.0, true, true);
        printf("[1]\n");
        gfx_draw_image(player_image, sprite, x+100,y+1, COLOR_TINT_NONE, 1.0, 0.0, 1.0, true, true);

        if(game_debug_enabled)
        {
            gfx_draw_rect_xywh(x,y,vw,vh, COLOR_BLUE, 0, 1.0, 1.0, false, true);
            gfx_draw_rect_xywh(x,y,32,32, COLOR_RED, 0, 1.0, 1.0, false, true);
            gfx_draw_rect_xywh(x,y,1,1, COLOR_GREEN, 0, 1.0, 1.0, false, true);
        }

        // for(int i = 0; i < 20; ++i)
        // {
        //     x += 32;
        //     y += 1;
        //     gfx_draw_image(player_image, sprite, x,y, COLOR_TINT_NONE, 1.0, 0.0, 1.0, false, true);
        // }

        // x += 100;
        // y += 1;
        // gfx_draw_image(player_image, sprite, x,y, COLOR_TINT_NONE, 1.0, 0.0, 1.0, false, true);
        // if(game_debug_enabled)
        // {
        //     gfx_draw_rect_xywh(x,y,vw,vh, COLOR_BLUE, 0, 1.0, 1.0, false, true);
        //     gfx_draw_rect_xywh(x,y,32,32, COLOR_RED, 0, 1.0, 1.0, false, true);
        //     gfx_draw_rect_xywh(x,y,1,1, COLOR_GREEN, 0, 1.0, 1.0, false, true);
        // }
    }
#endif


}

void run_settings()
{
    run_loop(SCREEN_SETTINGS, update_settings, draw_settings);
    settings_save();
}

void update_settings(float _dt, bool is_client)
{
    stars_update();
    text_list_update(text_lst, _dt);
}

void draw_settings(bool is_client)
{
    uint8_t r = background_color >> 16;
    uint8_t g = background_color >> 8;
    uint8_t b = background_color >> 0;

    gfx_clear_buffer(r,g,b);

    stars_draw();

    int x = (view_width-200)/2.0;
    int y = (view_height-200)/2.0;

    //float menu_item_scale = 0.4;

    //Vector2f s = gfx_draw_string(x,y, COLOR_WHITE, menu_item_scale, 0.0, 1.0, true, false, "Name");
    imgui_begin_panel("Settings", x,y, false);
        //imgui_set_text_size(menu_item_scale);
        imgui_text_box("Name", menu_settings.name, PLAYER_NAME_MAX);
        imgui_color_picker("Color", &menu_settings.color);
        GFXImage* img = &gfx_images[player_image];
        imgui_number_box("Sprite Index", 0, img->element_count-1, (int*)&menu_settings.sprite_index);
        imgui_newline();
        if(imgui_button("Return"))
        {
            screen = SCREEN_HOME;
        }
    imgui_end();

    gfx_draw_image_color_mask(player_image, menu_settings.sprite_index, view_width/2.0, 200, menu_settings.color, 3.0, 0.0, 1.0, true, true);

    text_list_draw(text_lst);

}

void run_game_start()
{
    bool is_client = (role == ROLE_CLIENT);

    initiate_game = false;
    client_id = -1;

    if(is_client)
    {
        net_client_init();
    }
    else
    {
        reset_game();
        player = &players[0];
        memcpy(&player->settings, &menu_settings, sizeof(Settings));
        player_init_local();
        players_set_ai_state();
    }

    run_loop(SCREEN_GAME_START, update_game_start, draw_game_start);

    if(screen != SCREEN_GAME)
        net_client_disconnect();
}

void update_game_start(float _dt, bool is_client)
{
    static float retry_time = 0.0;

    stars_update();
    text_list_update(text_lst, _dt);

    if(!is_client && initiate_game)
    {
        game_status = GAME_STATUS_RUNNING;
        players_set_ai_state();
        screen = SCREEN_GAME;
        return;
    }

    if(is_client)
    {
        if(client_id == -1)
        {

            if(net_client_get_state() == DISCONNECTED)
            {
                retry_time -= _dt;
                if(retry_time <= 0.0)
                {
                    net_client_connect_request();
                    text_list_add(text_lst, 2.0, "Sent connect request.");
                    retry_time = 2.0;
                }
            }

            else
            {
                int rc = net_client_connect_data_waiting();

                if(rc == 0)
                {
                    return;
                }
                else if(rc == 1)
                {
                    text_list_add(text_lst, 2.0, "Timedout waiting for data.");
                    return;
                }
                else if(rc == 2)
                {
                    int rcv = net_client_connect_recv_data();
                    if(rcv >= 0)
                    {
                        client_id = rcv;
                        text_list_add(text_lst, 5.0, "Connected, client id: %d.", rcv);

                        player = &players[client_id];
                        memcpy(&player->settings, &menu_settings, sizeof(Settings));
                        player_init_local();
                        net_client_send_settings();
                    }
                    else
                    {
                        if(rcv == CONN_RC_CHALLENGED)
                            text_list_add(text_lst, 2.0, "Received connect challenge.");
                        else if(rcv == CONN_RC_INVALID_SALT)
                            text_list_add(text_lst, 2.0, "Invalid salt.");
                        else if(rcv == CONN_RC_REJECTED)
                            text_list_add(text_lst, 2.0, "Connection rejected.");
                        else if(rcv == CONN_RC_NO_DATA)
                            text_list_add(text_lst, 2.0, "No data.");
                    }
                }
            }

        }
        else
        {
            net_client_update();
            simulate_client(_dt); // client-side prediction

            if(!net_client_is_connected())
            {
                text_list_add(text_lst, 2.0, "Got disconnected from server.");
                screen = SCREEN_HOME;
            }
        }

    }

}

void draw_game_start(bool is_client)
{
    uint8_t r = background_color >> 16;
    uint8_t g = background_color >> 8;
    uint8_t b = background_color >> 0;

    gfx_clear_lines();
    gfx_clear_buffer(r,g,b);

    stars_draw();

    if(is_client)
    {

        if(client_id == -1)
        {
            char* text = "Waiting for connection...";
            float title_scale = 1.0;
            Vector2f title_size = gfx_string_get_size(title_scale, text);
            gfx_draw_string((view_width-title_size.x)/2.0, (view_height-title_size.y)/4.0, COLOR_BLUE, title_scale, 0.0, 1.0, true, true, text);
        }
        else
        {

            // TODO: send messages
            imgui_begin_panel("Message", view_width/2.0,0, false);

                char* opts[] = {"0","1","2","3","4","5","6","7","ALL"};
                static int to_sel = 0;
                to_sel = imgui_dropdown(opts, MAX_PLAYERS+1, "Select Player", &to_sel);

                imgui_text("Message:");
                static char msg[255+1] = {0};
                imgui_text_box("##Messagebox", msg, 255);

                if(imgui_button("Send"))
                {
                    if(to_sel >= MAX_PLAYERS)
                        to_sel = TO_ALL;
                    net_client_send_message(to_sel, "%s", msg);
                }
            imgui_end();

            gfx_draw_rect(&ready_zone, COLOR_GREEN, 0.0, 1.0, 1.0, false, true);
            gfx_draw_string(ready_zone.x-ready_zone.w/2.0-10, ready_zone.y-ready_zone.h/2.0-10, COLOR_GREEN, 0.3, 0.0, 1.0, true, true, "READY");

            // projectiles
            // -----------------------------------------------------------------------
            for(int i = 0; i < plist->count; ++i)
            {
                projectile_draw(&projectiles[i]);
            }

            particles_draw_layer(0);

            // players
            // -----------------------------------------------------------------------
            for(int i = 0; i < MAX_PLAYERS; ++i)
            {
                Player* p = &players[i];
                player_draw(p);
            }

            particles_draw_layer(1);

            gfx_draw_lines();

            player_list_draw();
        }

    }
    else
    {
        int x = (view_width-200)/2.0;
        int y = (view_height-200)/2.0;

        imgui_begin_panel("Settings", x,y, false);
            imgui_number_box("Lives", 1, 10, &num_lives);
            imgui_number_box("Players", 2, MAX_PLAYERS, &num_players);
            imgui_toggle_button(&all_ai, "Toggle All AI");
            bool start = imgui_button("Start");
        imgui_end();

        if(start)
        {
            initiate_game = true;
        }

    }

    text_list_draw(text_lst);

}


void run_game_end()
{
    bool is_client = (role == ROLE_CLIENT);

    game_end_counter = 4.0;

    run_loop(SCREEN_GAME_END, update_game_end, draw_game_end);

    net_client_disconnect();
}

void update_game_end(float _dt, bool is_client)
{
    stars_update();
    text_list_update(text_lst, _dt);

    particles_update(_dt);
    player_update(&players[winner_index], _dt);

    game_end_counter -= _dt;
    if(game_end_counter <= 0.0)
    {
        if(role == ROLE_CLIENT)
        {
            screen = SCREEN_GAME_START;
        }
        else
        {
            screen = SCREEN_HOME;
        }
    }
}

void draw_game_end(bool is_client)
{
    uint8_t r = background_color >> 16;
    uint8_t g = background_color >> 8;
    uint8_t b = background_color >> 0;

    gfx_clear_buffer(r,g,b);
    gfx_clear_lines();

    gfx_draw_rect(&world_box, COLOR_BLACK, 0.0, 1.0, 1.0, false, true);

    stars_draw();

    player_draw(&players[winner_index]);

    char text[100] = {0};
    sprintf(text, "%s wins!", players[winner_index].settings.name);
    float title_scale = 1.0;
    Vector2f title_size = gfx_string_get_size(title_scale, text);
    int num_steps = 20;
    static int ci = -1;
    static uint32_t colors[60] = {0};
    if(ci == -1)
    {
        uint32_t color_list[3] = {COLOR_RED, COLOR_GREEN, COLOR_BLUE};
        gfx_color_gradient(color_list, 3, num_steps, colors);
        ci = 0;
    }
    gfx_draw_string((view_width-title_size.x)/2.0, (view_height-title_size.y)/4.0, colors[ci], title_scale, 0.0, 1.0, true, true, text);
    ci++;
    if(ci >= num_steps*3)
        ci = 0;

    // players
    // -----------------------------------------------------------------------
    for(int i = 0; i < MAX_CLIENTS; ++i)
    {
        Player* p = &players[i];
        player_draw(p);
    }

    text_list_draw(text_lst);
}


void simulate(double dt)
{
    if(!paused)
    {
        projectile_update(dt);
        powerups_update(dt);
        particles_update(dt);
        // stars_update();
        // text_list_update(text_lst, dt);

        window_get_mouse_view_coords(&mx, &my);

        if(role == ROLE_LOCAL)
        {
            bool leftc = window_mouse_left_went_down();
            bool rightc = window_mouse_right_went_down();
            if(leftc || rightc)
            {
                Rect r = {0};
                r.x = mx;
                r.y = my;
                r.w = 20;
                r.h = 20;
                for(int i = 0; i < MAX_PLAYERS; ++i)
                {
                    Player* p = &players[i];
                    if(player == p) continue;
                    if(!p->active) continue;
                    if(p->dead) continue;

                    if(rectangles_colliding(&r, &p->hit_box))
                    {
                        player_selection = p->id;
                        if(leftc)
                        {
                            player2 = p;
                            player_set_controls();

                            ParticleEffect e = mouse_click_effect;
                            // e.scale.init_min = 
                            e.color1 = COLOR(0,0xff,0);
                            e.color2 = e.color1;
                            e.color3 = e.color1;
                            particles_spawn_effect(mx,my,1, &e,1.0,true,false);
                            text_list_add(text_lst, 5.0, "Controlling %s", p->settings.name);

                        }

                        if(rightc)
                        {
                            p->ai = !p->ai;
                            if(p->ai)
                            {
                                ParticleEffect e = mouse_click_effect;
                                e.color1 = COLOR(0xff,0,0);
                                e.color2 = e.color1;
                                e.color3 = e.color1;
                                particles_spawn_effect(mx,my,1, &e,1.0,true,false);
                                text_list_add(text_lst, 5.0, "AI enabled for %s", p->settings.name);
                            }
                            else
                            {
                                p->actions[PLAYER_ACTION_SHOOT].state = false;

                                ParticleEffect e = mouse_click_effect;
                                e.color1 = COLOR(0,0,0xff);
                                e.color2 = e.color1;
                                e.color3 = e.color1;
                                particles_spawn_effect(mx,my,1, &e,1.0,true,false);
                                text_list_add(text_lst, 5.0, "AI disabled for %s", p->settings.name);
                            }
                        }
                    }
                }
            }
        }
    }

    // player_update(player, dt);
    for(int i = 0; i < MAX_PLAYERS; ++i)
    {
        player_update(&players[i], dt);
    }

    if(!paused) projectile_handle_collisions(dt);
}

void simulate_client(double dt)
{
    //projectile_update(dt);
    //player_update(player,dt); // client-side prediction

    particles_update(dt);

    player_handle_net_inputs(player, dt);

    for(int i = 0; i < plist->count; ++i)
    {
        projectile_lerp(&projectiles[i], dt);
        projectile_update_hit_box(&projectiles[i]);
    }

    for(int i = 0; i < MAX_CLIENTS; ++i)
    {
        if(players[i].active)
        {
            Player* p = &players[i];

            player_lerp(p, dt);
            player_update_hit_box(p);

            if(p->jets)
            {
                Rect* r = &gfx_images[player_image].visible_rects[p->settings.sprite_index];
                p->jets->pos.x = p->pos.x - 0.5*r->w*cosf(RAD(p->angle_deg));
                p->jets->pos.y = p->pos.y + 0.5*r->w*sinf(RAD(p->angle_deg));
            }
        }
    }

}


void run_game()
{

    if(role == ROLE_LOCAL)
    {
        player2 = &players[1];
        player_set_controls();
        for(int i = 0; i < num_players; ++i)
        {
            Player* p = &players[i];
            if(p == player) continue;
            p->active = true;
            // if(p != player2) p->ai = true;
        }
    }

    run_loop(SCREEN_GAME, update_game, draw_game);

    // net_client_disconnect();
}

void update_game(float _dt, bool is_client)
{
    if(!paused)
    {
        stars_update();
        text_list_update(text_lst, _dt);
    }

    if(is_client)
    {
        net_client_update();
        simulate_client(_dt); // client-side prediction

        if(!net_client_is_connected())
        {
            text_list_add(text_lst, 2.0, "Got disconnected from server.");
            screen = SCREEN_HOME;
        }
    }
    else
    {
        simulate(_dt);
    }
}

void sort_players(Player* lst[MAX_PLAYERS])
{
    // insertion sort
    int i, j;
    Player* key = NULL;
    for(i = 1; i < MAX_PLAYERS; i++) 
    {
        key = lst[i];
        j = i - 1;

        while(j >= 0 && lst[j]->deaths > (key)->deaths)
        {
            lst[j+1] = lst[j];
            j = j - 1;
        }
        lst[j+1] = key;
    }
}


void draw_game(bool is_client)
{
    uint8_t r = background_color >> 16;
    uint8_t g = background_color >> 8;
    uint8_t b = background_color >> 0;

    gfx_clear_buffer(r,g,b);
    gfx_clear_lines();

    gfx_draw_rect(&world_box, COLOR_BLACK, 0.0, 1.0, 1.0, false, true);

    stars_draw();


    // projectiles
    // -----------------------------------------------------------------------
    for(int i = 0; i < plist->count; ++i)
    {
        projectile_draw(&projectiles[i]);
    }

    // powerups
    // -----------------------------------------------------------------------
    powerups_draw();

    particles_draw_layer(0);


    // players
    // -----------------------------------------------------------------------
    for(int i = 0; i < MAX_PLAYERS; ++i)
    {
        Player* p = &players[i];
        player_draw(p);
    }

    particles_draw_layer(1);


    gfx_draw_lines();

    if(true)
    {
        player_list_draw();
    }

    text_list_draw(text_lst);

    if(game_debug_enabled)
    {
        gfx_draw_rect_xywh(mx, my, 10, 10, COLOR_RED, 0.0, 1.0, 1.0, false, true);
    }

    if(debug_enabled)
    {
        editor_draw();
    }

}


void run_server()
{
    deinit();
    init_server();
    net_server_start();
}

void stars_init()
{
    unsigned char data[4] = {0};
    for(int i = 0; i < 4; ++i)
        data[i] = 0xFF;
    stars_image = gfx_raw_image_create(data, 1, 1, false);

    for(int i = 0; i < NUM_STARS; ++i)
    {
        stars[i].x = rand() % view_width;
        stars[i].y = rand() % view_height;
        stars_size[i] = (rand() % 3) + 1;
    }
}

void stars_update()
{
    for(int i = 0; i < NUM_STARS; ++i)
    {
        stars[i].x -= (stars_size[i]);
        if(stars[i].x <= -5)
        {
            stars[i].x = view_width;
            stars[i].y = rand() % view_height;
            stars_size[i] = (rand() % 3) + 1;
        }
    }
}

void stars_draw()
{
    gfx_sprite_batch_begin(true);
    for(int i = 0; i < NUM_STARS; ++i)
    {
        gfx_sprite_batch_add(stars_image, 0, stars[i].x, stars[i].y, COLOR_WHITE, false, stars_size[i], 0.0, 1.0, true, true, false);
    }
    gfx_sprite_batch_draw();
}

void key_cb(GLFWwindow* window, int key, int scan_code, int action, int mods)
{
    // printf("key: %d, action: %d\n", key, action);
    bool ctrl = (mods & GLFW_MOD_CONTROL) == GLFW_MOD_CONTROL;

    KeyMode kmode = window_controls_get_key_mode();
    if(kmode == KEY_MODE_NORMAL)
    {
        if(action == GLFW_PRESS)
        {
            if(ctrl && key == GLFW_KEY_C)
            {
                window_set_close(1);
            }
            if(key == GLFW_KEY_ESCAPE)
            {

                if(debug_enabled)
                {
                    debug_enabled = false;
                }
                else if(screen != SCREEN_HOME)
                {
                    back_to_home = true;
                    screen = SCREEN_HOME;
                }
            }
            else if(key == GLFW_KEY_F2)
            {
                debug_enabled = !debug_enabled;
            }
            else if(key == GLFW_KEY_F3)
            {
                game_debug_enabled = !game_debug_enabled;
            }
            else if(key == GLFW_KEY_ENTER)
            {
                if(screen == SCREEN_GAME_START && role == ROLE_LOCAL)
                {
                    initiate_game = true;
                }
                // else if(screen == SCREEN_GAME_START && role == ROLE_CLIENT)
                // {
                //     printf("TEMP!\n");
                //     initiate_game = true;
                // }

            }

        }
    }
}

bool is_in_world(Rect* r)
{
    return rectangles_colliding(&world_box, r);
}

// rect is contained inside of limit
Vector2f limit_rect_pos(Rect* limit, Rect* rect)
{
    // printf("before: "); print_rect(rect);
    float lx0 = limit->x - limit->w/2.0;
    float lx1 = lx0 + limit->w;
    float ly0 = limit->y - limit->h/2.0;
    float ly1 = ly0 + limit->h;

    float px0 = rect->x - rect->w/2.0;
    float px1 = px0 + rect->w;
    float py0 = rect->y - rect->h/2.0;
    float py1 = py0 + rect->h;

    float _x = rect->x;
    float _y = rect->y;
    Vector2f adj = {0};

    if(px0 < lx0)
    {
        rect->x = lx0+rect->w/2.0;
        adj.x = rect->x - _x;
    }
    if(px1 > lx1)
    {
        rect->x = lx1-rect->w/2.0;
        adj.x = rect->x - _x;
    }
    if(py0 < ly0)
    {
        rect->y = ly0+rect->h/2.0;
        adj.y = rect->y - _y;
    }
    if(py1 > ly1)
    {
        rect->y = ly1-rect->h/2.0;
        adj.y = rect->y - _y;
    }
    // printf("after: "); print_rect(rect);

    // printf("adj: %.2f, %.2f\n", adj.x, adj.y);
    return adj;
}

void player_list_draw()
{
    Player* player_list[MAX_PLAYERS] = {0};
    for(int i = 0; i < MAX_PLAYERS; ++i)
        player_list[i] = &players[i];

    // printf("players    : ");
    // for(int i = 0; i < MAX_PLAYERS; ++i)
    //     printf("%p,  ", &players[i]);
    // printf("\n");

    // printf("before sort: ");
    // for(int i = 0; i < MAX_PLAYERS; ++i)
    //     printf("%p,  ", player_list[i]);
    // printf("\n");

    sort_players(player_list);

    // printf("after  sort: ");
    // for(int i = 0; i < MAX_PLAYERS; ++i)
    //     printf("%p,  ", player_list[i]);
    // printf("\n------------------------------------------------------------\n");

    float scale = 0.1;
    Vector2f size = gfx_string_get_size(scale, "P");
    float x = 10.0;
    float y = 10.0;
    for(int i = 0; i < MAX_PLAYERS; ++i)
    {
        Player* p = player_list[i];
        if(p->active)
        {
            uint32_t color = p->dead ? COLOR_RED : COLOR_WHITE;
            gfx_draw_string(x, y, color, scale, 0.0, 1.0, true, true, "%d | %s", p->deaths, p->settings.name);
            y += size.y + 3;
        }
    }
}
