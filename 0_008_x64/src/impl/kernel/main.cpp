#include "print.h"

extern "C" void kernel_main() {
    print_clear();
    print_set_color(PRINT_COLOR_LIGHT_RED, PRINT_COLOR_BLACK);
    print_str("Welcome to our TMLCS OS(NW OS V:0.0002a) 64-bit kernel on C++!");
}
