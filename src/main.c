#include <stdio.h>
#include <pc64k.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <math.h>
#ifndef M_PI
#define M_PI 3.1415926
#endif

#include <emu/config.h>

#ifdef __EMSCRIPTEN__
EM_JS(uint64_t, micros, (), {
    return BigInt(Math.floor(performance.now() * 1000))
});
#else
// https://stackoverflow.com/a/67731965
#include <time.h>
#define SEC_TO_US(sec) ((sec)*1000000)
#define NS_TO_US(ns)    ((ns)/1000)
uint64_t micros() {
    struct timespec ts;
    clock_gettime(TIME_UTC, &ts);
    uint64_t us = SEC_TO_US((uint64_t) ts.tv_sec) + NS_TO_US((uint64_t) ts.tv_nsec);
    return us;
}
#endif

PC64K* pc64k;

// uint8_t rom[] = {
//     /* 0x00 */ 0x06, 0x00, 0x20, // Set R0 to 0x20 (ASCII 32)
//     /* 0x03 */ 0x1a, 0x00, 0x7f, 0x00, 0x24, // If R0 = 0x7f (last printable ASCII char + 1), go to 0x24
//     /* 0x08 */ 0x24, 0x00, // Print R0 to the screen
//     /* 0x0a */ 0x06, 0x10, 0x01, // R0++
//     /* 0x0d */ 0x03, 0x01, // Copy R0 to R1
//     /* 0x0f */ 0x06, 0x61, 0x0f, // Mask R1 with 0b00001111
//     /* 0x12 */ 0x1a, 0x10, 0x00, 0x00, 0x1d, // If R1 == 0 (which means R0 % 16 == 0), go to 0x1d
//     /* 0x17 */ 0x00, 0x00, 0x03, // Go to 0x03
//     /* 0x1a */ 0x00, 0x00, 0x1a, // Go to 0x1a (loop)
//     /* 0x1d */ 0x1c, 0x0d, // Print \r
//     /* 0x1f */ 0x1c, 0x0a, // Print \n
//     /* 0x21 */ 0x00, 0x00, 0x03, // Go to 0x03
//     /* 0x24 */ 0x06, 0x02, 0xe0, // Set R2 to 0xe0 (red)
//     /* 0x27 */ 0x06, 0x03, 0x1c, // Set R3 to 0x1c (green)
//     /* 0x2a */ 0x20, 0x23, // Set colors to R2/R3
//     /* 0x2c */ 0x00, 0x00, 0x1a, // Go to 0x1a
// };
// uint8_t rom[] = {
//     /* 0x00 */ 0x06, 0x10, 0x01, // R0++
//     /* 0x03 */ 0x25, 0x00, 0x00, 0x0a, // If R0 is pressed, go to 0x0a
//     /* 0x07 */ 0x00, 0x00, 0x00, // Go to 0x00
//     /* 0x0a */ 0x24, 0x00, // Print R0
//     /* 0x0c */ 0x00, 0x00, 0x00, // Go to 0x00
// };
uint8_t rom[] = {
    /* 0x00 */ 0x06, 0x00, 0x41, // Set R0 to 'a'
    /* 0x03 */ 0x06, 0x01, 0x01, // Set R1 to 1
    /* 0x06 */ 0x1b, 0x01, // Set delay timer frequency to R1
    /* 0x08 */ 0x21, // Clears display
    /* 0x09 */ 0x24, 0x00, // Prints system character R0
    /* 0x0b */ 0x1b, 0x21, // Set delay timer value to R1
    /* 0x0d */ 0x1b, 0x40, // Wait for delay timer to finish
    /* 0x0f */ 0x06, 0x10, 0x01, // R0++
    /* 0x12 */ 0x00, 0x00, 0x08, // Goes to 0x08
};

// https://gist.github.com/fabiovila/b7626dcc0208a4d60abfbb047d4050bc
SDL_AudioDeviceID device;
uint32_t sample_pos;
static void gen_sine(void* data, uint8_t* buffer, int length) {
    for(int i = 0; i < length; i++)
        buffer[i] = 128 + 30 * sin((double) sample_pos++ / (SAMPLE_FREQ / BEEP_FREQ) * M_PI * 2);
}

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
bool last_sound_on = false;

static void loop() {
    SDL_Event evt;
    while(SDL_PollEvent(&evt)) {
        if(evt.type == SDL_KEYDOWN)
            pc64k_setkey(pc64k, evt.key.keysym.sym, true);
        else if(evt.type == SDL_KEYUP)
            pc64k_setkey(pc64k, evt.key.keysym.sym, false);
    }
    uint64_t now = micros();
    uint64_t interval = (double) 1000000 / CPU_SPEED_HZ;
    while(now - last >= interval) {
        pc64k_tick(pc64k);
        last += interval;
    }
    if(last_sound_on && pc64k->sound.value == 0) {
        last_sound_on = false;
        SDL_PauseAudioDevice(device, true);
    } else if(!last_sound_on && pc64k->sound.value != 0) {
        last_sound_on = true;
        SDL_PauseAudioDevice(device, false);
    }
    update_surface(surface);
    SDL_UpdateWindowSurface(window);
}

bool init_audio() {
    SDL_AudioSpec iscapture, desired;
    SDL_zero(iscapture);
    iscapture.freq = SAMPLE_FREQ;
    iscapture.format = AUDIO_U8;
    iscapture.channels = 1;
    iscapture.samples = 2048;
    iscapture.callback = gen_sine;
    
    device = SDL_OpenAudioDevice(NULL, 0, &iscapture, &desired, SDL_AUDIO_ALLOW_FORMAT_CHANGE);
    if(!device) return false;
    SDL_PauseAudioDevice(device, true);
    return true;
}

int main() {
    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0)
        return 1;
    if(!init_audio())
        return 1;
    window = SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, DISPLAY_WIDTH, DISPLAY_HEIGHT, SDL_WINDOW_SHOWN);
    if(window == NULL) return 1;
    surface = SDL_GetWindowSurface(window);

    pc64k = pc64k_alloc_init(rom, sizeof(rom), get_disk_size, read_disk, write_disk, micros);

    last = micros();
    #ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(loop, 0, 1);
    #else
    while(true) loop();
    #endif
    // pc64k_deinit_free(pc64k);

    // SDL_DestroyWindow(window);
    // SDL_Quit();
    return 0;
}