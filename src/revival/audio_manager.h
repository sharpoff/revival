#pragma once

#include <miniaudio.h>

class AudioManager
{
public:
    bool init();
    void shutdown();
    void playSound(const char *filename);
    void setMasterVolume(float db);
private:
    ma_engine engine;
};
