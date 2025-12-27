#include "cpu/pic.h"
#include <io.h> // For outb and inb

// PIC ports
#define PIC1_COMMAND    0x20
#define PIC1_DATA       0x21
#define PIC2_COMMAND    0xA0
#define PIC2_DATA       0xA1

// EOI command
#define PIC_EOI         0x20

namespace PIC {

    void remap(int offset_master, int offset_slave) {
        uint8_t a1, a2;

        a1 = inb(PIC1_DATA); // save masks
        a2 = inb(PIC2_DATA);

        outb(PIC1_COMMAND, 0x11); // starts the initialization sequence (ICW1)
        outb(PIC2_COMMAND, 0x11);

        outb(PIC1_DATA, offset_master); // ICW2: Master PIC vector offset
        outb(PIC2_DATA, offset_slave);  // ICW2: Slave PIC vector offset

        outb(PIC1_DATA, 0x04); // ICW3: tell Master PIC there is a slave PIC at IRQ2 (0000 0100)
        outb(PIC2_DATA, 0x02); // ICW3: tell Slave PIC its cascade identity (0000 0010)

        outb(PIC1_DATA, 0x01); // ICW4: 8086 mode
        outb(PIC2_DATA, 0x01);

        outb(PIC1_DATA, a1); // restore saved masks
        outb(PIC2_DATA, a2);
    }

    void disable() {
        outb(PIC1_DATA, 0xFF);
        outb(PIC2_DATA, 0xFF);
    }

    void send_eoi(uint8_t irq) {
        if (irq >= 8) {
            outb(PIC2_COMMAND, PIC_EOI);
        }
        outb(PIC1_COMMAND, PIC_EOI);
    }
}
