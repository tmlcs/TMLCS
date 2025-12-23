#include "print.h"


typedef void (*constructor)();
extern "C" constructor start_ctors;
extern "C" constructor end_ctors;

extern "C" void call_ctors() {
    for (constructor* i = &start_ctors; i != &end_ctors; i++) {
        (*i)();
    }
}

extern "C" void kernel_main(const void* multiboot_structure, unsigned int /* magic number*/) {
    (void)multiboot_structure; // Suppress unused parameter warning
    print_clear();
    print_set_color(PRINT_COLOR_LIGHT_RED, PRINT_COLOR_BLACK);
    print_str("Welcome to GLOBEX by TMLCS (NW OS V:0.0002a) 64-bit kernel");
    asm volatile ("hlt");
}
