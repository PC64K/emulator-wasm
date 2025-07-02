#include <stdio.h>
#include <pc64k.h>
#include <emscripten.h>
#include <SDL2/SDL.h>

#include <emu/config.h>

EM_JS(uint64_t, micros, (), {
    return BigInt(Math.floor(performance.now() * 1000))
});

PC64K* pc64k;

uint8_t rom[] = {
    /* 0x00 */ 0x06, 0x00, 0x20, // Set R0 to 0x20 (ASCII 32)
    /* 0x03 */ 0x1a, 0x00, 0x7f, 0x00, 0x24, // If R0 = 0x7f (last printable ASCII char + 1), go to 0x24
    /* 0x08 */ 0x24, 0x00, // Print R0 to the screen
    /* 0x0a */ 0x06, 0x10, 0x01, // R0++
    /* 0x0d */ 0x03, 0x01, // Copy R0 to R1
    /* 0x0f */ 0x06, 0x61, 0x0f, // Mask R1 with 0b00001111
    /* 0x12 */ 0x1a, 0x10, 0x00, 0x00, 0x1d, // If R1 == 0 (which means R0 % 16 == 0), go to 0x1d
    /* 0x17 */ 0x00, 0x00, 0x03, // Go to 0x03
    /* 0x1a */ 0x00, 0x00, 0x1a, // Go to 0x1a (loop)
    /* 0x1d */ 0x1c, 0x0d, // Print \r
    /* 0x1f */ 0x1c, 0x0a, // Print \n
    /* 0x21 */ 0x00, 0x00, 0x03, // Go to 0x03
    /* 0x24 */ 0x06, 0x02, 0xe0, // Set R2 to 0xe0 (red)
    /* 0x27 */ 0x06, 0x03, 0x1c, // Set R3 to 0x1c (green)
    /* 0x2a */ 0x20, 0x23, // Set colors to R2/R3
    /* 0x2c */ 0x00, 0x00, 0x1a, // Go to 0x1a
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

static SDL_Window* window;
static SDL_Surface* surface;

uint64_t last;

static void loop() {
    SDL_Event evt;
    while(SDL_PollEvent(&evt)) {
        if(evt.type == SDL_KEYDOWN)
            printf("%c\n", evt.key.keysym.sym);
    }
    uint64_t now = micros();
    uint64_t ticks = ((float) (now - last) / 1000000) * CPU_SPEED_HZ;
    for(uint64_t i = 0; i < ticks; i++) pc64k_tick(pc64k);
    last = now;
    update_surface(surface);
    SDL_UpdateWindowSurface(window);
}

int main() {
    if(SDL_Init(SDL_INIT_VIDEO) != 0) // might have to add SDL_INIT_EVENTS later
        return 1;
    window = SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, DISPLAY_WIDTH, DISPLAY_HEIGHT, SDL_WINDOW_SHOWN);
    if(window == NULL) return 1;
    surface = SDL_GetWindowSurface(window);

    pc64k = pc64k_alloc_init(rom, sizeof(rom), get_disk_size, read_disk, write_disk, micros);

    last = micros();
    emscripten_set_main_loop(loop, 0, 1);
    // pc64k_deinit_free(pc64k);

    // SDL_DestroyWindow(window);
    // SDL_Quit();
    return 0;
}