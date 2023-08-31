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

GameRole role;
bool paused = false;
Timer game_timer = {0};

bool debug_enabled = false;

// =========================
// Function Prototypes
// =========================

void parse_args(int argc, char* argv[]);
void init();
void deinit();
void start_local();
void start_client();
void start_server();
void simulate(double);
void simulate_client(double);
void draw();

// =========================
// Main Loop
// =========================

int main(int argc, char* argv[])
{
    init_timer();
    log_init(0);

    parse_args(argc, argv);

    switch(role)
    {
        case ROLE_LOCAL:
            start_local();
            break;
        case ROLE_CLIENT:
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
    role = ROLE_LOCAL;

    if(argc > 1)
    {
        for(int i = 1; i < argc; ++i)
        {
            if(argv[i][0] == '-' && argv[i][1] == '-')
            {
                // server
                if(strncmp(argv[i]+2,"server",6) == 0)
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

void start_local()
{
    LOGI("--------------");
    LOGI("Starting Local");
    LOGI("--------------");

    time_t t;
    srand((unsigned) time(&t));

    init();

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
    LOGI("---------------");
    LOGI("Starting Client");
    LOGI("---------------");

    time_t t;
    srand((unsigned) time(&t));

    timer_set_fps(&game_timer,TARGET_FPS);
    timer_begin(&game_timer);

    net_client_init();

    int client_id = net_client_connect();
    if(client_id < 0)
        return;

    LOGN("Client ID: %d", client_id);
    player = &players[client_id];

    init();

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
            simulate_client(dt); // client-side prediction
            net_client_update();
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
    LOGI("---------------");
    LOGI("Starting Server");
    LOGI("---------------");

    time_t t;
    srand((unsigned) time(&t));

    view_width = VIEW_WIDTH;
    view_height = VIEW_HEIGHT;

    // server init
    //gfx_image_init(); // todo
    player_init();
    //projectile_init();

    net_server_start();
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

    LOGI("Initializing...");

    LOGI(" - Shaders.");
    shader_load_all();

    LOGI(" - Graphics.");
    gfx_init(VIEW_WIDTH, VIEW_HEIGHT);

    LOGI(" - Player.");
    player_init();
    player_init_other(1);

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
    projectile_update(dt);
    player_update(player,dt);
}

static char lines[100][100+1] = {0};
static int line_count = 0;
static uint32_t background_color = 0x00303030;

int num_clicks = 0;
float v1 = 0.0;

void draw()
{
    uint8_t r = background_color >> 16;
    uint8_t g = background_color >> 8;
    uint8_t b = background_color >> 0;

    gfx_clear_buffer(r,g,b);

    // projectiles
    // -----------------------------------------------------------------------
    for(int i = 0; i < plist->count; ++i)
    {
        projectile_draw(&projectiles[i]);
    }

    // players
    // -----------------------------------------------------------------------
    for(int i = 0; i < MAX_PLAYERS; ++i)
    {
        player_draw(&players[i]);
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
