#include "interrupts.h"
#include "log.h"
#include "print.h"

extern "C" void InterruptHandler(InterruptStackFrame* stack_frame) {
    (void)stack_frame; // Suppress unused parameter warning
    Log::error("UNHANDLED INTERRUPT!");
    Log::error("Interrupt Number: ");
    // TODO: Convert stack_frame->interrupt_number to string for logging
    // Log::error(stack_frame->interrupt_number);

    // For now, just print to VGA
    print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_RED);
    print_str("UNHANDLED INTERRUPT: ");
    // print_char(stack_frame->interrupt_number + '0'); // This will only work for single digit interrupts
    print_str("\n");

    // Halt the CPU
    asm volatile ("hlt");
}
