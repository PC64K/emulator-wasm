#include <stdio.h>
#include <pc64k.h>
#include <emscripten.h>
#include <SDL2/SDL.h>

EM_JS(uint64_t, micros, (), {
    return Math.floor(performance.now() * 1000)
});

PC64K* pc64k;

uint8_t rom[] = {
    0x06, 0x00, 0x20,
    0x1a, 0x00, 0x7f, 0x00, 0x10,
    0x24, 0x00,
    0x06, 0x10, 0x01,
    0x00, 0x00, 0x03,
    0x00, 0x00, 0x10
};

size_t get_disk_size() {
    return 0;
}
void read_disk(size_t pos, uint8_t* data, uint8_t len) {
    memset(data + pos, 0, len);
}
void write_disk(size_t pos, uint8_t* data, uint8_t len) {}

void update_surface(SDL_Surface* surface) {
    for(uint16_t x = 0; x < DISPLAY_WIDTH; x++)
        for(uint8_t y = 0; y < DISPLAY_HEIGHT; y++) {
            uint32_t* target = (uint32_t*) ((uint8_t*) surface->pixels + y * surface->pitch + x * surface->format->BytesPerPixel);
            uint32_t rgba = (((pc64k->video.framebuffer[x][y] >> 5) & 0b111) << 5)
                | (((pc64k->video.framebuffer[x][y] >> 2) & 0b111) << 13)
                | (((pc64k->video.framebuffer[x][y]) & 0b11) << 22)
                | 0x00;
            *target = rgba;
        }
}

int main() {
    if(SDL_Init(SDL_INIT_VIDEO) != 0) // might have to add SDL_INIT_EVENTS later
        return 1;
    SDL_Window* window = SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, DISPLAY_WIDTH, DISPLAY_HEIGHT, SDL_WINDOW_SHOWN);
    if(window == NULL) return 1;
    SDL_Surface* surface = SDL_GetWindowSurface(window);

    pc64k = pc64k_alloc_init(rom, sizeof(rom), get_disk_size, read_disk, write_disk, micros);
    for(int i = 0; i < 10000; i++) {
        pc64k_tick(pc64k);
        update_surface(surface);
        SDL_UpdateWindowSurface(window);
        // SDL_Delay(10);
    }
    pc64k_deinit_free(pc64k);

    // SDL_DestroyWindow(window);
    // SDL_Quit();
    return 0;
}