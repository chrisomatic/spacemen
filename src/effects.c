#include "io.h"
#include "log.h"
#include "effects.h"

ParticleEffect particle_effects[EFFECT_MAX];
int num_effects = 0;

EffectEntry effect_map[] = {
    {EFFECT_GUN_SMOKE1,"gun_smoke.effect"},
    {EFFECT_SPARKS1,"sparks1.effect"},
    {EFFECT_HEAL1,"heal1.effect"},
    {EFFECT_BLOOD1,"blood1.effect"},
    {EFFECT_DEBRIS1,"debris1.effect"},
    {EFFECT_MELEE1,"melee1.effect"},
    {EFFECT_BULLET_CASING,"bullet_casing.effect"},
    {EFFECT_FIRE,"fire.effect"},
    {EFFECT_BLOCK_DESTROY,"block_destroy.effect"},
    {EFFECT_SMOKE,"smoke.effect"},
    {EFFECT_SMOKE2,"smoke2.effect"},
    {EFFECT_BULLET_TRAIL,"bullet_trail.effect"},
    {EFFECT_GUN_BLAST,"gun_blast.effect"},
    {EFFECT_LEVEL_UP,"level_up.effect"},
    {EFFECT_JETS,"jets.effect"},
    {EFFECT_EXPLOSION,"explosion.effect"},
    {EFFECT_PLAYER_ACTIVATE,"player_activate.effect"}
};

static int get_effect_map_index(char* file_name)
{
    int num_effects_in_map = sizeof(effect_map)/sizeof(EffectEntry);
    for(int i = 0; i < num_effects_in_map; ++i)
    {
        if(strcmp(effect_map[i].file_name, file_name) == 0)
        {
            return i;
        }
    }
    return -1;
}

void effects_load_all()
{
    char files[32][32] = {0};
    num_effects = io_get_files_in_dir("src/effects",".effect", files);

    LOGI("Num effects: %d",num_effects);

    for(int i = 0; i < num_effects; ++i)
    {
        char* filename = io_get_filename(files[i]);
        char full_path[100] = {0};
        snprintf(full_path,99,"src/effects/%s",filename);
        printf("files[%d]: %s\n",i, filename);
        int index = get_effect_map_index(filename);
        printf("Loading %s into index %d\n",filename, index);
        if(index == -1)
        {
            LOGW("Failed to map effect %s",filename);
        }
        else if(index >= EFFECT_MAX)
        {
            LOGW("Map effect is out of max particles range, %d",index);
        }
        else
        {
            effects_load(full_path,&particle_effects[index]);
            strncpy(particle_effects[index].name,filename,100);

            LOGI("%d: %s",i,filename);
        }
    }
}

bool effects_save(char* file_path, ParticleEffect* effect)
{
    FILE* fp = fopen(file_path,"wb");
    if(!fp)
    {
        LOGW("Failed to save file %s\n",file_path);
        return false;
    }

    fwrite(effect,sizeof(ParticleEffect),1,fp);
    LOGI("Saved effect: %s",file_path);
    fclose(fp);
    return true;
}

bool effects_load(char* file_path, ParticleEffect* effect)
{
    FILE* fp = fopen(file_path,"rb");
    if(!fp)
    {
        LOGW("Failed to load file %s\n",file_path);
        return false;
    }

    fread(effect,sizeof(ParticleEffect),1,fp);
    LOGI("Loaded effect: %s", file_path);
    fclose(fp);
    return true;
}
