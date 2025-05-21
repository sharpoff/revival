#include <cstdio>
#include <revival/audio_manager.h>

bool AudioManager::init()
{
    ma_result result = ma_engine_init(NULL, &engine);
    if (result != MA_SUCCESS)
        return false;

    return true;
}

void AudioManager::shutdown()
{
    ma_engine_uninit(&engine);
}

void AudioManager::playSound(const char *filename)
{
    ma_engine_play_sound(&engine, filename, NULL);
}

void AudioManager::setMasterVolume(float db)
{
    ma_engine_set_volume(&engine, ma_volume_db_to_linear(db));
}
