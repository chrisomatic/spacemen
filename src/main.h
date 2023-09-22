#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include "log.h"
#include "timer.h"
#include "math2d.h"
#include "settings.h"
#include "core/text_list.h"

typedef enum
{
    ROLE_UNKNOWN,
    ROLE_LOCAL,
    ROLE_CLIENT,
    ROLE_SERVER,
} GameRole;

typedef enum
{
    SCREEN_SERVER,
    SCREEN_HOME,
    SCREEN_SETTINGS,
    SCREEN_GAME_START,
    SCREEN_GAME,
    SCREEN_GAME_END,

    SCREEN_MAX
} DisplayScreen;

#define DEBUG_PROJ_GRIDS    0

#define VIEW_WIDTH   1200
#define VIEW_HEIGHT  800

// players, zombies, items
#define IMG_ELEMENT_W 128
#define IMG_ELEMENT_H 128

// strings
#define STR_EMPTY(x)      (x == 0 || strlen(x) == 0)
#define STR_EQUAL(x,y)    (strncmp((x),(y),strlen((x))) == 0 && strlen(x) == strlen(y))
#define STRN_EQUAL(x,y,n) (strncmp((x),(y),(n)) == 0)

#define FREE(p) do{ if(p != NULL) {free(p); p = NULL;} }while(0)

#define DEBUG_PRINT()   printf("%d %s %s()\n", __LINE__, __FILE__, __func__)

extern text_list_t* text_lst;

extern DisplayScreen screen;
extern bool initialized;
extern bool back_to_menu;
extern bool paused;
extern bool debug_enabled;
extern bool game_debug_enabled;
extern int num_lives;
extern int num_players;
extern float game_end_counter;
extern int winner_index;

extern Timer game_timer;
extern GameRole role;
extern Rect world_box;
extern Settings menu_settings;

extern bool all_active;
extern bool all_ai;

bool is_in_world(Rect* r);
Vector2f limit_rect_pos(Rect* limit, Rect* rect);
