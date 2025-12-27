#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>

namespace Serial {
    void init();
    bool is_transmit_empty();
    void write_char(char c);
    void write_string(const char* str);
}

#endif // SERIAL_H
