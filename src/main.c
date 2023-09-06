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

bool initialized = false;
bool back_to_menu = false;
bool paused = false;
bool debug_enabled = false;
Timer game_timer = {0};
GameRole role;
Rect world_box = {0};



Vector2i stars[1000] = {0};
int stars_size[1000] = {0};

// =========================
// Function Prototypes
// =========================

void parse_args(int argc, char* argv[]);
void init();
void init_server();
void deinit();
void start_menu();
void start();
void start_server();
void simulate(double);
void simulate_client(double);
void draw();
void draw_menu();

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

    parse_args(argc, argv);

    time_t t;
    srand((unsigned) time(&t));


    for(;;)
    {

        if(back_to_menu)
        {
            role = ROLE_UNKNOWN;
            back_to_menu = false;
        }

        switch(role)
        {
            case ROLE_UNKNOWN:
                init();
                stars_init();
                start_menu();
                break;
            case ROLE_LOCAL:
                init();
                start();
                break;
            case ROLE_CLIENT:
                init();
                start();
                break;
            case ROLE_SERVER:
                deinit();
                start_server();
                break;
        }

        if(window_should_close())
            break;
    }

    printf("return\n");
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

typedef struct
{
    bool up;
    bool down;
    bool enter;
} MenuKeys;

MenuKeys menu_keys_prior = {0};
MenuKeys menu_keys = {0};

void start_menu()
{
    window_controls_clear_keys();
    window_controls_add_key(&menu_keys.up,    GLFW_KEY_W);
    window_controls_add_key(&menu_keys.down,  GLFW_KEY_S);
    window_controls_add_key(&menu_keys.enter, GLFW_KEY_ENTER);

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

        stars_update();
        draw_menu();

        if(role != ROLE_UNKNOWN)
            break;

        timer_wait_for_frame(&game_timer);
        window_swap_buffers();
        window_mouse_update_actions();
    }
}

void start()
{
    bool is_client = (role == ROLE_CLIENT);

    timer_set_fps(&game_timer,TARGET_FPS);
    timer_begin(&game_timer);

    stars_init();

    if(is_client)
    {
        net_client_init();

        int client_id = net_client_connect();
        if(client_id < 0)
            return;

        LOGN("Client ID: %d", client_id);
        player = &players[client_id];
    }
    else
    {
        player = &players[0];
    }

    player_init_local();

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

        if(back_to_menu)
            break;

        if(is_client)
        {
            if(!net_client_is_connected())
                break;
        }

        while(accum >= dt)
        {
            if(is_client)
            {
                net_client_update();
                simulate_client(dt); // client-side prediction
            }
            else
            {
                simulate(dt);
            }
            accum -= dt;
        }

        draw();

        timer_wait_for_frame(&game_timer);
        window_swap_buffers();
        window_mouse_update_actions();
    }

    if(is_client)
    {
        net_client_disconnect();
    }

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

    world_box.w = view_width;
    world_box.h = view_height;
    world_box.x = view_width/2.0;
    world_box.y = view_height/2.0;

    //gfx_image_init(); // todo
    for(int i = 0; i < MAX_CLIENTS; ++i)
    {
        players[i].active = false;
        player_init(&players[i]);
    }

    projectile_init();

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
}

void deinit()
{
    if(!initialized) return;
    initialized = false;
    shader_deinit();
    window_deinit();
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

uint32_t background_color = 0x00303030;
int menu_selected_option = 0;



void draw_menu()
{
    if(menu_keys.down)
    {
        menu_keys.down = false;
        menu_selected_option++;
        if(menu_selected_option > 4) menu_selected_option = 0;
    }

    if(menu_keys.up)
    {
        menu_keys.up = false;
        menu_selected_option--;
        if(menu_selected_option < 0) menu_selected_option = 4;
    }

    if(menu_keys.enter)
    {
        menu_keys.enter = false;
        switch(menu_selected_option)
        {
            case 0: role = ROLE_LOCAL; return;
            case 1: role = ROLE_CLIENT; net_client_set_server_ip("127.0.0.1"); return;
            case 2: role = ROLE_CLIENT; net_client_set_server_ip("66.228.36.123"); return;
            case 3: role = ROLE_SERVER; return;
            case 4: exit(0);
            default: break;
        }
    }

    memcpy(&menu_keys_prior, &menu_keys, sizeof(MenuKeys));

    uint8_t r = background_color >> 16;
    uint8_t g = background_color >> 8;
    uint8_t b = background_color >> 0;

    gfx_clear_buffer(r,g,b);

    stars_draw();


    int num_steps = 80;
    static int ci = -1;
    static uint32_t colors[40] = {0};
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
    gfx_draw_string(x,y, menu_selected_option == 1 ? COLOR_YELLOW : COLOR_WHITE, menu_item_scale, 0.0, 1.0, true, false, "Join Local Server"); y += 32;
    gfx_draw_string(x,y, menu_selected_option == 2 ? COLOR_YELLOW : COLOR_WHITE, menu_item_scale, 0.0, 1.0, true, false, "Join Public Server"); y += 32;
    gfx_draw_string(x,y, menu_selected_option == 3 ? COLOR_YELLOW : COLOR_WHITE, menu_item_scale, 0.0, 1.0, true, false, "Host Server"); y += 32;
    gfx_draw_string(x,y, menu_selected_option == 4 ? COLOR_YELLOW : COLOR_WHITE, menu_item_scale, 0.0, 1.0, true, false, "Exit"); y += 32;

    gfx_draw_image(player_image, 0, x - 34,(view_height+100)/2.0 + (32*menu_selected_option) + 16, COLOR_TINT_NONE, 1.0, 0.0, 1.0, true, true);

    /*
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
    */
}

void draw()
{
    uint8_t r = background_color >> 16;
    uint8_t g = background_color >> 8;
    uint8_t b = background_color >> 0;

    gfx_clear_buffer(r,g,b);

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

    if(debug_enabled)
    {
        imgui_draw_demo(10,10);

        imgui_begin_panel("Theme",500,10, false);
            imgui_color_picker("Window Background Color", &background_color);
            imgui_theme_editor();
        imgui_end();
    }
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
            if(key == GLFW_KEY_ESCAPE)
            {
                back_to_menu = true;
            }
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
