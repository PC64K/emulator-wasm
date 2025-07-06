#include <emu/kb.h>

static uint8_t get_key(SDL_Event* evt) {
    SDL_Keycode code = evt->key.keysym.sym;
    uint16_t mod = evt->key.keysym.mod;
    // Assuming US keyboard
    if(mod & KMOD_SHIFT) {
        if(code >= 'a' && code <= 'z') return code - 0x20;

        if(code == '0') return ')';
        if(code == '1') return '!';
        if(code == '2') return '@';
        if(code == '3') return '#';
        if(code == '4') return '$';
        if(code == '5') return '%';
        if(code == '6') return '^';
        if(code == '7') return '&';
        if(code == '8') return '*';
        if(code == '9') return '(';

        if(code == '`') return '~';
        if(code == '-') return '_';
        if(code == '=') return '+';
        if(code == '[') return '{';
        if(code == ']') return '}';
        if(code == '\\') return '|';
        if(code == ';') return ':';
        if(code == '\'') return '"';
        if(code == ',') return '<';
        if(code == '.') return '>';
        if(code == '/') return '?';
    }
    return code;
}

void handle_event(SDL_Event* evt, PC64K* pc64k) {
    if(evt->key.keysym.sym == SDLK_LSHIFT || evt->key.keysym.sym == SDLK_RSHIFT)
        return; // Do not handle is the only key held is SHIFT
    if(evt->type == SDL_KEYDOWN)
        pc64k_setkey(pc64k, get_key(evt), true);
    else if(evt->type == SDL_KEYUP)
        pc64k_setkey(pc64k, get_key(evt), false);
}