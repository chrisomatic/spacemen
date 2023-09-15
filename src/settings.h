#pragma once

#define PLAYER_NAME_MAX 16

typedef struct
{
    char name[PLAYER_NAME_MAX+1];
    uint32_t color;
    uint8_t sprite_index;
} Settings;

bool settings_save();
bool settings_load();
