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
#include <emu/kb.h>

#ifdef __EMSCRIPTEN__
EM_JS(uint64_t, micros, (), {
    return BigInt(Math.floor(performance.now() * 1000))
});
EM_ASYNC_JS(void, load_rom, (size_t size, uint8_t* buf), {
    if(location.hash) {
        HEAPU8.set(Uint8Array.from(location.hash.replace("#", "").match(/.{1,2}/g).map((byte) => parseInt(byte, 16))), buf);
        document.querySelector("#dialog").style.display = "none";
        return;
    }
    const romEl = document.querySelector("#rom");
    await new Promise(resolve => romEl.addEventListener("change", resolve));
    const reader = new FileReader();
    let done = false;
    reader.addEventListener("load", () => {
        HEAPU8.set(new Uint8Array(reader.result).subarray(0, size), buf);
        done = true;
    });
    reader.readAsArrayBuffer(romEl.files[0]);
    const check = async resolve => done ? resolve() : setTimeout(check, 1, resolve);
    await new Promise(check);
    document.querySelector("#dialog").style.display = "none";
});

size_t get_disk_size() {
    return 65536; // Always 64KB
}
EM_JS(void, read_disk, (size_t pos, uint8_t* data, uint8_t len), {
    const no = document.querySelector("#disk").value;
    const sub = (localStorage.getItem("__64_" + no) || "\0".repeat(65536)).split("").map(x => x.charCodeAt(0)).slice(pos, pos + len);
    for(let i = 0; i < len; i++)
        HEAPU8[data + i] = sub[i];
});
EM_JS(void, write_disk, (size_t pos, uint8_t* data, uint8_t len), {
    const no = document.querySelector("#disk").value;
    const sub = HEAPU8.subarray(data, data + len);
    const disk = (localStorage.getItem("__64_" + no) || "\0".repeat(65536)).split("").map(x => x.charCodeAt(0));
    for(let i = 0; i < len; i++)
        disk[pos + i] = sub[i];
    localStorage.setItem("__64_" + no, disk.map(x => String.fromCharCode(x)).join(""));
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
void load_rom(size_t size, uint8_t* buf);

size_t get_disk_size() {
    return 0;
}
void read_disk(size_t pos, uint8_t* data, uint8_t len) {
    memset(data + pos, 0, len);
}
void write_disk(size_t pos, uint8_t* data, uint8_t len) {}
#endif

PC64K* pc64k;

#define ROM_SIZE 65536
uint8_t rom[ROM_SIZE];

// https://gist.github.com/fabiovila/b7626dcc0208a4d60abfbb047d4050bc
SDL_AudioDeviceID device;
uint32_t sample_pos;
static void gen_sine(void* data, uint8_t* buffer, int length) {
    for(int i = 0; i < length; i++)
        buffer[i] = 128 + 30 * sin((double) sample_pos++ / (SAMPLE_FREQ / BEEP_FREQ) * M_PI * 2);
}

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
    while(SDL_PollEvent(&evt)) 
        handle_event(&evt, pc64k);
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
    load_rom(ROM_SIZE, rom);

    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0)
        return 1;
    if(!init_audio())
        return 1;
    window = SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, DISPLAY_WIDTH, DISPLAY_HEIGHT, SDL_WINDOW_SHOWN);
    if(window == NULL) return 1;
    surface = SDL_GetWindowSurface(window);

    pc64k = pc64k_alloc_init(rom, ROM_SIZE, get_disk_size, read_disk, write_disk, micros);

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