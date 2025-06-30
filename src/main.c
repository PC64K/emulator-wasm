#include <stdio.h>
#include <pc64k.h>
#include <emscripten.h>

EM_JS(uint64_t, micros, (), {
    return Math.floor(performance.now() * 1000)
});

PC64K* pc64k;

uint8_t rom[] = {
    0x1c, 0x41,
    0x00, 0x00, 0x00
};

size_t get_disk_size() {
    return 0;
}
uint8_t* read_disk(size_t pos, uint8_t len) {
    uint8_t* data = (uint8_t*)malloc(len);
    memset(data, 0, len);
    return data;
}
void write_disk(size_t pos, uint8_t* data, uint8_t len) {}

int main() {
    pc64k = pc64k_alloc_init(rom, sizeof(rom), get_disk_size, read_disk, write_disk, micros);
    pc64k_deinit_free(pc64k);
    return 0;
}