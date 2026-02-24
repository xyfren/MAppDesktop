#include "VideoDisplay.h"

VideoDisplay::VideoDisplay(int w, int h) : width(w), height(h) {
    //SDL_SetMainReady();
    SDL_Init(SDL_INIT_VIDEO);
    window = SDL_CreateWindow("Video Display",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        width, height, SDL_WINDOW_SHOWN);

    renderer = SDL_CreateRenderer(window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    texture = SDL_CreateTexture(renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        width, height);
}


void VideoDisplay::updateFrame(void* pixels) {
    SDL_UpdateTexture(texture, NULL, pixels, width * 4);
    // Здесь же можно сразу отрисовать, но обычно отрисовка в цикле
}

ID3D11Device* VideoDisplay::GetD3D11Device() {
    return SDL_RenderGetD3D11Device(renderer);
}

// Отрисовка текущего состояния (вызывается каждый кадр отображения)
void VideoDisplay::render() {
    // Обработка событий
    SDL_Event e;
    while (SDL_PollEvent(&e)) { if (e.type == SDL_QUIT); }/* exit */
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}

void VideoDisplay::wait() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) { if (e.type == SDL_QUIT); }
}


VideoDisplay::~VideoDisplay() {
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}