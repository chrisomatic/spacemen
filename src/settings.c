#include <stdio.h>
#include <stdlib.h>

#include "main.h"
#include "player.h"
#include "settings.h"

const char* settings_file = "spacemen.settings";

bool settings_save()
{
    FILE* fp = fopen(settings_file, "w");

    if(!fp)
        return false;

    fprintf(fp, "name: %s\n", menu_settings.name);
    fprintf(fp, "color: %08X\n", menu_settings.color);
    fprintf(fp, "sprite_index: %u\n", menu_settings.sprite_index);

    fclose(fp);

    return true;
}

bool settings_load()
{
    FILE* fp = fopen(settings_file, "r");

    if(!fp)
        return false;

    char label[100] = {0};
    char value[100] = {0};

    int lindex = 0;
    int vindex = 0;

    bool is_value = false;

    int c;

    for(;;)
    {
        c = fgetc(fp);

        if(c == EOF)
            break;

        if(c == ' ' || c == '\t')
            continue; // whitespace, don't care
        
        if(is_value && c == '\n')
        {
            is_value = !is_value;
            
            if(STR_EQUAL(label, "name"))
            {
                memset(menu_settings.name,0,PLAYER_NAME_MAX);
                memcpy(menu_settings.name,value,MIN(PLAYER_NAME_MAX, vindex));
            }
            else if(STR_EQUAL(label, "color"))
            {
                menu_settings.color = (uint32_t)strtol(value, NULL, 16);
            }
            else if(STR_EQUAL(label, "sprite_index"))
            {
                menu_settings.sprite_index = atoi(value);
            }

            memset(label,0,100); lindex = 0;
            memset(value,0,100); vindex = 0;

            continue;
        }

        if(!is_value && c == ':')
        {
            is_value = !is_value;
            label[lindex++] == c;
            continue;
        }

        if((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || (c == '-' || c == '_'))
        {
            // valid character

            if(is_value) value[vindex++] = c;
            else         label[lindex++] = c;
        }

    }

    return true;

}
