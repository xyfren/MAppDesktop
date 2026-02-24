#pragma once

#include <SDL2/SDL.h>

class VideoDisplay {
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* texture;
    int width, height;

public:
    VideoDisplay(int w, int h);

    ID3D11Device* GetD3D11Device();

    void updateFrame(void* pixels);
    void render();
    void wait();

    ~VideoDisplay();
};