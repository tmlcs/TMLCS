#include "print.h"

static void* _memmove(void* dst, const void* src, size_t n) {
    unsigned char* d = (unsigned char*)dst;
    const unsigned char* s = (const unsigned char*)src;
    if (d < s) {
        while (n--) {
            *d++ = *s++;
        }
    } else {
        const unsigned char* lasts = s + (n - 1);
        unsigned char* lastd = d + (n - 1);
        while (n--) {
            *lastd-- = *lasts--;
        }
    }
    return dst;
}

const static size_t NUM_COLS = 80;
const static size_t NUM_ROWS = 25;

struct Char {
    uint8_t character;
    uint8_t color;
};

struct Char* buffer = (struct Char*)0xb8000;
size_t col = 0;
size_t row = 0;
uint8_t color = PRINT_COLOR_WHITE | (PRINT_COLOR_BLACK << 4);

static void* _memsetw(void* buf, int c, size_t n) {
    unsigned short* p = (unsigned short*)buf;
    while (n--) {
        *p++ = (unsigned short)c;
    }
    return buf;
}

void clear_row(size_t row) {
    unsigned short empty = ' ' | (color << 8);
    _memsetw(&buffer[NUM_COLS * row], empty, NUM_COLS);
}

void print_clear() {
    for (size_t i = 0; i < NUM_ROWS; i++) {
        clear_row(i);
    }
}

void print_newline() {
    col = 0;

    if (row < NUM_ROWS - 1) {
        row++;
        return;
    }

    _memmove(&buffer[0], &buffer[NUM_COLS], NUM_COLS * (NUM_ROWS - 1) * 2);
    clear_row(NUM_ROWS - 1);
}

void print_char(char character) {
    if (character == '\n') {
        print_newline();
        return;
    }

    if (col >= NUM_COLS) {
        print_newline();
    }

    buffer[col + NUM_COLS * row] = {
        static_cast<uint8_t>(character),
        color
    };

    col++;
}

void print_str(const char* str) {
    while (*str != '\0') {
        print_char(*str++);
    }
}

void print_set_color(uint8_t foreground, uint8_t background) {
    color = foreground + (background << 4);
}
