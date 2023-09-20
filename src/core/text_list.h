#pragma once

typedef enum
{
    TEXT_ALIGN_LEFT,
    TEXT_ALIGN_RIGHT,
    TEXT_ALIGN_CENTER
} text_align_t;

typedef struct
{
    int max;
    int count;
    char** text;
    float* durations;
    bool downward;
    text_align_t alignment;
    float x;
    float y;
    float scale;
    uint32_t color;
} text_list_t;



text_list_t* text_list_init(int max, float x, float y, float scale, uint32_t color, bool downward, text_align_t alignment);
void text_list_add(text_list_t* lst, float duration, char* fmt, ...);
void text_list_update(text_list_t* lst, float _dt);
void text_list_draw(text_list_t* lst);
