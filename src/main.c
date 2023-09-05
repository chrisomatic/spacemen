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

// =========================
// Global Vars
// =========================

bool paused = false;
bool debug_enabled = false;
Timer game_timer = {0};
GameRole role;
Rect world_box = {0};



// =========================
// Function Prototypes
// =========================

void parse_args(int argc, char* argv[]);
void init();
void init_server();
void deinit();
void start_menu();
void start_local();
void start_client();
void start_server();
void simulate(double);
void simulate_client(double);
void draw();
void draw_menu();

void key_cb(GLFWwindow* window, int key, int scan_code, int action, int mods);

// =========================
// Main Loop
// =========================

int main(int argc, char* argv[])
{
    init_timer();
    log_init(0);

    parse_args(argc, argv);

    time_t t;
    srand((unsigned) time(&t));

    switch(role)
    {
        case ROLE_UNKNOWN:
            init();
            start_menu();
            break;
        case ROLE_LOCAL:
            init();
            start_local();
            break;
        case ROLE_CLIENT:
            init();
            start_client();
            break;
        case ROLE_SERVER:
            start_server();
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
                    role = ROLE_LOCAL;

                // server
                else if(strncmp(argv[i]+2,"server",6) == 0)
                    role = ROLE_SERVER;

                // client
                else if(strncmp(argv[i]+2,"client",6) == 0)
                    role = ROLE_CLIENT;
            }
            else
            {
                net_client_set_server_ip(argv[i]);
            }
        }
    }
}

void start_menu()
{
    timer_set_fps(&game_timer,TARGET_FPS);
    timer_begin(&game_timer);

    double curr_time = timer_get_time();
    double new_time  = 0.0;
    double accum = 0.0;

    const double dt = 1.0/TARGET_FPS;

    // main game loop
    for(;;)
    {
        new_time = timer_get_time();
        double frame_time = new_time - curr_time;
        curr_time = new_time;

        accum += frame_time;

        window_poll_events();
        if(window_should_close())
            break;

        while(accum >= dt)
            accum -= dt;

        draw_menu();

        if(role != ROLE_UNKNOWN)
            break;

        timer_wait_for_frame(&game_timer);
        window_swap_buffers();
        window_mouse_update_actions();
    }

    switch(role)
    {
        case ROLE_LOCAL:
            start_local();
            break;
        case ROLE_CLIENT:
            start_client();
            break;
        case ROLE_SERVER:
            deinit();
            start_server();
            break;
    }
}

void start_local()
{
    timer_set_fps(&game_timer,TARGET_FPS);
    timer_begin(&game_timer);

    double curr_time = timer_get_time();
    double new_time  = 0.0;
    double accum = 0.0;

    const double dt = 1.0/TARGET_FPS;

    // main game loop
    for(;;)
    {
        new_time = timer_get_time();
        double frame_time = new_time - curr_time;
        curr_time = new_time;

        accum += frame_time;

        window_poll_events();
        if(window_should_close())
            break;

        while(accum >= dt)
        {
            simulate(dt);
            accum -= dt;
        }

        draw();

        timer_wait_for_frame(&game_timer);
        window_swap_buffers();
        window_mouse_update_actions();
    }

    deinit();
}

void start_client()
{
    timer_set_fps(&game_timer,TARGET_FPS);
    timer_begin(&game_timer);

    net_client_init();

    int client_id = net_client_connect();
    if(client_id < 0)
        return;

    LOGN("Client ID: %d", client_id);
    player = &players[client_id];

    double curr_time = timer_get_time();
    double new_time  = 0.0;
    double accum = 0.0;

    const double dt = 1.0/TARGET_FPS;

    // main game loop
    for(;;)
    {
        new_time = timer_get_time();
        double frame_time = new_time - curr_time;
        curr_time = new_time;

        accum += frame_time;

        window_poll_events();
        if(window_should_close())
            break;

        if(!net_client_is_connected())
            break;

        while(accum >= dt)
        {
            net_client_update();
            simulate_client(dt); // client-side prediction
            accum -= dt;
        }

        draw();

        timer_wait_for_frame(&game_timer);
        window_swap_buffers();
        window_mouse_update_actions();
    }

    net_client_disconnect();
    deinit();
}

void start_server()
{

    init_server();
    net_server_start();
}

void init_server()
{
    view_width = VIEW_WIDTH;
    view_height = VIEW_HEIGHT;

    //gfx_image_init(); // todo
    for(int i = 0; i < MAX_CLIENTS; ++i)
    {
        player_init(&players[i]);
    }

    projectile_init();


}

void init()
{
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

    LOGI(" - Player.");
    player_init(player);
    // player_init_other(1);

    LOGI(" - Projectile.");
    projectile_init();
}

void deinit()
{
    shader_deinit();
    window_deinit();
}

void simulate(double dt)
{
    if(!paused) projectile_update(dt);

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
    player_handle_net_inputs(player, dt);

    for(int i = 0; i < plist->count; ++i)
    {
        projectile_lerp(&projectiles[i], dt);
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

static uint32_t background_color = 0x00303030;

void draw_menu()
{
    uint8_t r = background_color >> 16;
    uint8_t g = background_color >> 8;
    uint8_t b = background_color >> 0;

    gfx_clear_buffer(r,g,b);

    for(int i = 0; i < 1000; ++i)
    {

        int rx = rand() % view_width;
        int ry = rand() % view_height;
        int s = (rand() % 5) + 1;

        gfx_draw_rect_xywh(rx, ry, 1, 1, COLOR_WHITE, 0.0, s, 1.0, true, true);
    }

    uint8_t tr = rand() % 256;
    uint8_t tg = rand() % 256;
    uint8_t tb = rand() % 256;

    float title_scale = 3.0;
    Vector2f title_size = gfx_string_get_size(title_scale, "SPACEMEN");

    gfx_draw_string((view_width-title_size.x)/2.0, (view_height-title_size.y)/4.0, COLOR(tr,tg,tb), title_scale, 0.0, 1.0, true, true, "SPACEMEN");

    imgui_begin_panel("Options",(view_width-200)/2.0,(view_height+100)/2.0, false);
        if(imgui_button("Play Local"))
        {
            role = ROLE_LOCAL;
        }
        else if(imgui_button("Join Local Server"))
        {
            net_client_set_server_ip("127.0.0.1");
            role = ROLE_CLIENT;
        }
        else if(imgui_button("Join Public Server"))
        {
            net_client_set_server_ip("66.228.36.123");
            role = ROLE_CLIENT;
        }
        else if(imgui_button("Host Server"))
        {
            role = ROLE_SERVER;
        }
        else if(imgui_button("Exit"))
        {
            exit(0);
        }
    imgui_end();
}

void draw()
{
    uint8_t r = background_color >> 16;
    uint8_t g = background_color >> 8;
    uint8_t b = background_color >> 0;

    gfx_clear_buffer(r,g,b);

    gfx_draw_rect(&world_box, COLOR_BLACK, 0.0, 1.0, 1.0, false, true);

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

    if(debug_enabled)
    {
        imgui_draw_demo(10,10);

        imgui_begin_panel("Theme",500,10, false);
            imgui_color_picker("Window Background Color", &background_color);
            imgui_theme_editor();
        imgui_end();
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
            // if(key == GLFW_KEY_ESCAPE)
            // {
            //     window_enable_cursor();
            // }
            if(ctrl && key == GLFW_KEY_C)
            {
                window_set_close(1);
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
