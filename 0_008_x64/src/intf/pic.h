#ifndef PIC_H
#define PIC_H

#include <stdint.h>

namespace PIC {
    void remap(int offset_master, int offset_slave);
    void disable();
    void send_eoi(uint8_t irq);
}

#endif // PIC_H
