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
#include "settings.h"


/*
TODO:
1) handle disconnection from server

*/

// =========================
// Global Vars
// =========================

bool initialized = false;
bool back_to_home = false;
bool paused = false;
bool debug_enabled = false;
bool game_debug_enabled = false;
bool initiate_game = false;

int client_id = -1;

Timer game_timer = {0};
GameRole role;
Rect world_box = {0};

// Settings
uint32_t background_color = 0x00303030;
int menu_selected_option = 0;

Settings menu_settings = {0};

Vector2i stars[1000] = {0};
int stars_size[1000] = {0};

typedef enum
{
    SCREEN_SERVER,
    SCREEN_HOME,
    SCREEN_SETTINGS,
    SCREEN_GAME_START,
    SCREEN_GAME,

    SCREEN_MAX
} DisplayScreen;
DisplayScreen screen = SCREEN_HOME;

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
void simulate(double);
void simulate_client(double);

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

void run_game();
void update_game(float _dt, bool is_client);
void draw_game(bool is_client);

void run_server();

void stars_init();
void stars_update();
void stars_draw();

void key_cb(GLFWwindow* window, int key, int scan_code, int action, int mods);

// =========================
// Main Loop
// =========================

int main(int argc, char* argv[])
{

    init_timer();
    log_init(0);

    screen = SCREEN_HOME;
    parse_args(argc, argv);

    time_t t;
    srand((unsigned) time(&t));

    if(role != ROLE_SERVER)
        init();

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
        }

        if(window_should_close())
            break;
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

    LOGI(" - Players.");
    for(int i = 0; i < MAX_CLIENTS; ++i)
    {
        players[i].active = false;
        player_init(&players[i]);
    }

    LOGI(" - Projectile.");
    projectile_init();

    LOGI(" - Settings.");
    settings_load();
    memcpy(&player->settings, &menu_settings, sizeof(Settings));

    imgui_load_theme("retro.theme");

    stars_init();
}

void init_server()
{
    view_width = VIEW_WIDTH;
    view_height = VIEW_HEIGHT;

    world_box.w = view_width;
    world_box.h = view_height;
    world_box.x = view_width/2.0;
    world_box.y = view_height/2.0;

    gfx_image_init();
    for(int i = 0; i < MAX_CLIENTS; ++i)
    {
        players[i].active = false;
        player_init(&players[i]);
    }

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
    for(int i = 0; i < MAX_CLIENTS; ++i)
    {
        players[i].active = false;
        player_init(&players[i]);
    }
}


void simulate(double dt)
{
    if(!paused)
    {
        projectile_update(dt);
        stars_update();
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
    stars_update();
    //projectile_update(dt);
    //player_update(player,dt); // client-side prediction
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
            player_lerp(&players[i], dt);
            player_update_hit_box(&players[i]);
        }
    }

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
            break;

        if(screen != _screen)
            break;

        while(accum >= dt)
        {
            if(_update != NULL) _update(dt, is_client);
            accum -= dt;
        }
        if(_draw != NULL) _draw(is_client);

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
        imgui_text_box("Name", menu_settings.name, 100);
        imgui_color_picker("Color", &menu_settings.color);
        GFXImage* img = &gfx_images[player_image];
        imgui_number_box("Sprite Index", 0, img->element_count-1, &menu_settings.sprite_index);
        imgui_newline();
        if(imgui_button("Return"))
        {
            screen = SCREEN_HOME;
        }
    imgui_end();

    gfx_draw_image_color_mask(player_image, menu_settings.sprite_index, view_width/2.0, 200, menu_settings.color, 3.0, 0.0, 1.0, true, true);

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

    run_loop(SCREEN_GAME_START, update_game_start, draw_game_start);

    if(screen != SCREEN_GAME)
        net_client_disconnect();
}

void update_game_start(float _dt, bool is_client)
{

    stars_update();

    // if(!is_client && initiate_game)
    if(!is_client)
    {
        reset_game();
        player = &players[0];

        memcpy(&player->settings, &menu_settings, sizeof(Settings));
        player_init_local();
        player_init_local2();

        screen = SCREEN_GAME;
        return;
    }

    if(is_client)
    {
        if(client_id == -1)
        {

            if(net_client_get_state() == DISCONNECTED)
            {
                net_client_connect_request();
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
                    return;
                }
                else if(rc == 2)
                {
                    int _id = net_client_connect_recv_data();
                    if(_id >= 0)
                    {
                        client_id = _id;
                    }
                }
            }

        }

        if(client_id != -1)
        {
            LOGN("Client ID: %d", client_id);
            player = &players[client_id];

            memcpy(&player->settings, &menu_settings, sizeof(Settings));
            player_init_local();

            net_client_send_settings();

            screen = SCREEN_GAME;
            return;
        }

    }

}

void draw_game_start(bool is_client)
{
    uint8_t r = background_color >> 16;
    uint8_t g = background_color >> 8;
    uint8_t b = background_color >> 0;

    gfx_clear_buffer(r,g,b);

    stars_draw();


    char* text = "press enter";

    if(is_client)
    {
        if(net_client_get_state() == DISCONNECTED)
        {
            text = "sending connect...";
        }
        else
        {
            text = "waiting...";
        }
    }
    // printf("text: %s\n", text);

    float title_scale = 1.0;
    Vector2f title_size = gfx_string_get_size(title_scale, text);
    gfx_draw_string((view_width-title_size.x)/2.0, (view_height-title_size.y)/4.0, COLOR_BLUE, title_scale, 0.0, 1.0, true, true, text);
}


void run_game()
{
    run_loop(SCREEN_GAME, update_game, draw_game);

    net_client_disconnect();
}

void update_game(float _dt, bool is_client)
{
    if(is_client)
    {
        net_client_update();
        simulate_client(_dt); // client-side prediction

        if(!net_client_is_connected())
        {
            screen = SCREEN_HOME;
        }
    }
    else
    {
        simulate(_dt);
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

    // players
    // -----------------------------------------------------------------------
    for(int i = 0; i < MAX_CLIENTS; ++i)
    {
        Player* p = &players[i];
        if(p->active)
            player_draw(p);
    }


    gfx_draw_lines();

    if(debug_enabled)
    {
        imgui_draw_demo(10,10);

        imgui_begin_panel("Theme",500,10, false);
            imgui_color_picker("Window Background Color", &background_color);
            imgui_theme_editor();
        imgui_end();
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
    for(int i = 0; i < 1000; ++i)
    {
        stars[i].x = rand() % view_width;
        stars[i].y = rand() % view_height;
        stars_size[i] = (rand() % 3) + 1;
    }
}

void stars_update()
{
    for(int i = 0; i < 1000; ++i)
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
    for(int i = 0; i < 1000; ++i)
    {
        gfx_draw_rect_xywh(stars[i].x, stars[i].y, 1, 1, COLOR_WHITE, 0, stars_size[i], 1.0, true, true);
    }
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
                if(screen != SCREEN_HOME)
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
