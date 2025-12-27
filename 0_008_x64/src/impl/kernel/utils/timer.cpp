#include "utils/timer.h"
#include "cpu/interrupts.h"
#include "utils/log.h"
#include "io.h" // For outb

#define PIT_CHANNEL0_DATA 0x40
#define PIT_COMMAND_PORT  0x43

static uint32_t tick = 0;

void timer_handler(InterruptStackFrame* stack_frame) {
    (void)stack_frame; // Suppress unused parameter warning
    tick++;
    // Log::printf(Log::INFO, "Timer tick: %d", tick); // Too verbose for now
}

namespace Timer {
    void init(uint32_t frequency) {
        Interrupts::RegisterInterruptHandler(0x20, timer_handler); // IRQ0 is mapped to 0x20

        uint32_t divisor = 1193180 / frequency;
        outb(PIT_COMMAND_PORT, 0x36); // Command byte: 0x36 = Channel 0, LSB/MSB, Rate Generator mode
        outb(PIT_CHANNEL0_DATA, divisor & 0xFF); // LSB
        outb(PIT_CHANNEL0_DATA, (divisor >> 8) & 0xFF); // MSB

        Log::info("Timer initialized with frequency: %d Hz", frequency);
    }
}
