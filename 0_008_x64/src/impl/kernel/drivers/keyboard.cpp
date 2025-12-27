#include "drivers/keyboard.h"
#include "cpu/interrupts.h"
#include "utils/log.h"
#include "io.h" // For inb, outb

#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_COMMAND_PORT 0x64

// Scancode to ASCII mapping (simplified for common keys)
// This table only covers a subset of keys and assumes a standard US layout.
// It does not handle shifts, caps lock, or other modifiers yet.
static const char scancode_to_ascii[] = {
    0,  0,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', 0,
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0, 'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ', 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

// Circular buffer for keyboard input
#define KEYBOARD_BUFFER_SIZE 256
static char keyboard_buffer[KEYBOARD_BUFFER_SIZE];
static volatile uint32_t buffer_head = 0;
static volatile uint32_t buffer_tail = 0;

// Function to wait for keyboard controller to be ready for command
static void keyboard_wait_for_command() {
    while ((inb(KEYBOARD_COMMAND_PORT) & 0x02) != 0);
}

// Enqueue a character into the buffer
static void enqueue_char(char c) {
    uint32_t next_head = (buffer_head + 1) % KEYBOARD_BUFFER_SIZE;
    if (next_head != buffer_tail) { // If buffer is not full
        keyboard_buffer[buffer_head] = c;
        buffer_head = next_head;
    } else {
        Log::warning("Keyboard buffer full, character dropped.");
    }
}

// Dequeue a character from the buffer
static char dequeue_char() {
    if (buffer_head == buffer_tail) { // If buffer is empty
        return 0; // Or some other indicator of empty
    }
    char c = keyboard_buffer[buffer_tail];
    buffer_tail = (buffer_tail + 1) % KEYBOARD_BUFFER_SIZE;
    return c;
}

namespace Keyboard {

    static void KeyboardHandler(InterruptStackFrame* stack_frame) {
        (void)stack_frame; // Mark parameter as unused
        uint8_t scancode = inb(KEYBOARD_DATA_PORT);

        // Only process key-down events (bit 7 is 0 for key-down)
        if (!(scancode & 0x80)) {
            if (scancode < sizeof(scancode_to_ascii)) {
                char ascii = scancode_to_ascii[scancode];
                if (ascii != 0) {
                    enqueue_char(ascii);
                    Log::info("Keyboard: Key pressed: %c (scancode: %x)", ascii, scancode);
                }
            }
        }
    }

    void init() {
        // Disable keyboard
        keyboard_wait_for_command();
        outb(KEYBOARD_COMMAND_PORT, 0xAD);

        // Flush output buffer
        inb(KEYBOARD_DATA_PORT);

        // Enable keyboard
        keyboard_wait_for_command();
        outb(KEYBOARD_COMMAND_PORT, 0xAE);

        // Set keyboard LED status (optional, just to test communication)
        keyboard_wait_for_command();
        outb(KEYBOARD_COMMAND_PORT, 0x60); // Write command byte
        keyboard_wait_for_command();
        outb(KEYBOARD_DATA_PORT, 0x00); // All LEDs off

        Interrupts::RegisterInterruptHandler(0x21, KeyboardHandler); // IRQ1 is mapped to 0x21
        Log::info("Keyboard driver initialized.");
    }

    char get_char() {
        return dequeue_char();
    }
}
