#include "print.h"
#include "string.h"
#include "constants.h"

// ==========================================
// Memory Barrier Macro
// ==========================================
// Previene que el compilador/CPU reordene accesos a memoria
// ESENCIAL para hardware MMIO y variables compartidas
#define memory_barrier() __asm__ volatile ("" ::: "memory")

// ==========================================
// Estructura de caracter VGA (packed para 2 bytes)
// ==========================================
struct VgaChar {
    uint8_t character;
    uint8_t color;

    // Constructor para facilitar asignación
    constexpr VgaChar() : character(0), color(0) {}
    constexpr VgaChar(uint8_t c, uint8_t col) : character(c), color(col) {}
};

// ==========================================
// Buffer de video - VOLATILE para hardware MMIO
// ==========================================
static volatile VgaChar* vga_buffer = reinterpret_cast<volatile VgaChar*>(VGA_BUFFER_ADDRESS);

// ==========================================
// Estado del hardware VGA
// ==========================================
// IMPORTANTE: Estas variables NO son thread-safe para SMP.
// Para futuro soporte SMP/multitarea, considerar:
// 1. Usar atomic operations (si disponibles en freestanding)
// 2. Implementar per-CPU cursor state
// 3. Proteger con spinlock
//
// NOTA: volatile previene optimización del compilador pero NO
// garantiza atomicidad en sistemas multi-core.
static volatile bool vga_detected = false;
static volatile size_t cursor_col = 0;
static volatile size_t cursor_row = 0;
static uint8_t current_color = (PRINT_COLOR_WHITE & 0x0F) | ((PRINT_COLOR_BLACK & 0x0F) << 4);

// ==========================================
// Funciones auxiliares
// ==========================================

// Calcular índice lineal en el buffer
static inline size_t vga_index(size_t row, size_t col) {
    return row * VGA_COLS + col;
}

// Validar límites antes de acceder
static inline bool is_valid_position(size_t row, size_t col) {
    return (row < VGA_ROWS) && (col < VGA_COLS);
}

// Validar rango de color VGA (0-15)
static inline bool is_valid_color(uint8_t color) {
    return color <= 15;
}

// ==========================================
// Detección de hardware VGA
// ==========================================

bool print_detect(void) {
    // Asumir VGA disponible por defecto para kernel temprano
    // La detección real requiere acceso a BIOS que puede no estar disponible
    // en todos los entornos de emulación/hardware
    
    // Test de escritura/lectura seguro en VGA buffer
    volatile VgaChar* test_ptr = &vga_buffer[0];
    uint8_t saved_char = test_ptr->character;
    uint8_t saved_color = test_ptr->color;
    
    test_ptr->character = 0xAA;
    test_ptr->color = 0x55;
    
    // Verificar que se escribió correctamente
    bool exists = (test_ptr->character == 0xAA && test_ptr->color == 0x55);
    
    // Restaurar valores originales
    test_ptr->character = saved_char;
    test_ptr->color = saved_color;
    
    vga_detected = exists;
    return exists;
}

// ==========================================
// Implementación de funciones públicas
// ==========================================

void clear_row(size_t row) {
    if (!vga_detected || row >= VGA_ROWS) {
        return;  // Validación: VGA debe estar detectado y row válido
    }

    // Optimización: escribir ambos bytes (character + color) como un solo u16
    // VGA text mode: cada celda es 2 bytes (char: low byte, color: high byte)
    // Esto reduce los accesos a memoria de 160 (80*2) a 80 writes u16
    const uint16_t clear_word = static_cast<uint16_t>(' ') | (static_cast<uint16_t>(current_color) << 8);
    
    // reinterpret_cast del buffer como uint16_t* para writes de 2 bytes
    volatile uint16_t* row_ptr = reinterpret_cast<volatile uint16_t*>(&vga_buffer[vga_index(row, 0)]);
    
    for (size_t c = 0; c < VGA_COLS; c++) {
        row_ptr[c] = clear_word;
    }
    memory_barrier();  // Prevenir reordering después de writes a hardware
}

void print_clear(void) {
    if (!vga_detected) {
        return;  // Silently fail si no hay VGA detectado
    }

    for (size_t r = 0; r < VGA_ROWS; r++) {
        clear_row(r);
    }
    
    // Reset cursor con memory barrier para sincronización
    cursor_col = 0;
    cursor_row = 0;
    memory_barrier();
}

void print_newline(void) {
    if (!vga_detected) {
        return;  // VGA no detectado
    }

    // Lectura volatile explícita con memory barrier
    memory_barrier();
    cursor_col = 0;
    memory_barrier();

    if (cursor_row < VGA_ROWS - 1) {
        cursor_row++;
        memory_barrier();
        return;
    }

    // Scroll: mover filas 1..24 a filas 0..23
    // Optimización: copiar fila por fila usando accesos u16 (2 bytes por acceso)
    // En lugar de 4 accesos byte por acceso, usamos 1 acceso u16 por columna
    
    // Copiar filas 1..24 a filas 0..23
    for (size_t r = 0; r < VGA_ROWS - 1; r++) {
        volatile uint16_t* dst = reinterpret_cast<volatile uint16_t*>(&vga_buffer[vga_index(r, 0)]);
        const volatile uint16_t* src = reinterpret_cast<const volatile uint16_t*>(&vga_buffer[vga_index(r + 1, 0)]);
        
        // Copiar 80 columnas × 2 bytes = 80 u16 accesses
        for (size_t c = 0; c < VGA_COLS; c++) {
            dst[c] = src[c];
        }
    }
    memory_barrier();

    clear_row(VGA_ROWS - 1);
    memory_barrier();
}

void print_char(char character) {
    // Leer cursor con memory barrier para sincronización
    memory_barrier();
    size_t col = cursor_col;
    size_t row = cursor_row;

    switch (character) {
        case '\n':
            print_newline();
            return;
        case '\r':
            cursor_col = 0;
            memory_barrier();
            return;
        case '\t':
            // Avanzar al próximo tab stop (cada 8 columnas)
            col = (col + 8) & ~7;
            if (col >= VGA_COLS) {
                cursor_row++;
                cursor_col = 0;
                memory_barrier();
                print_newline();
            } else {
                cursor_col = col;
                memory_barrier();
            }
            return;
        default:
            break;
    }

    if (col >= VGA_COLS) {
        print_newline();
        // print_newline ya actualiza cursor, leer de nuevo
        memory_barrier();
        col = cursor_col;
        row = cursor_row;
    }

    if (is_valid_position(row, col)) {
        // Escritura directa para evitar problemas con volatile
        vga_buffer[vga_index(row, col)].character = static_cast<uint8_t>(character);
        vga_buffer[vga_index(row, col)].color = current_color;
        cursor_col = col + 1;
        memory_barrier();  // Asegurar que el write se complete antes de continuar
    }
}

void print_str(const char* str) {
    if (str == nullptr) {
        return;  // Validación de puntero nulo
    }

    // Limitar longitud máxima para prevenir writes infinitos
    constexpr size_t MAX_PRINT_LEN = 4096;

    for (size_t i = 0; i < MAX_PRINT_LEN && str[i] != '\0'; i++) {
        print_char(str[i]);
    }
    memory_barrier();  // Asegurar que todos los writes se completen
}

void print_set_color(uint8_t foreground, uint8_t background) {
    // Validación de rango (0-15) para colores VGA
    // Si los valores están fuera de rango, usar defaults (white on black)
    if (!is_valid_color(foreground) || !is_valid_color(background)) {
        // Valores inválidos - usar default y retornar
        current_color = (PRINT_COLOR_WHITE & 0x0F) | ((PRINT_COLOR_BLACK & 0x0F) << 4);
        memory_barrier();
        return;
    }
    
    // Valores válidos - aplicar máscara y combinar
    current_color = (foreground & 0x0F) | ((background & 0x0F) << 4);
    memory_barrier();  // Asegurar que el cambio de color se propague
}

void print_hex(uint32_t value) {
    static const char hex_chars[] = "0123456789ABCDEF";
    char buffer[11];  // "0x" + 8 digits + null = 11 bytes
    int i;

    buffer[0] = '0';
    buffer[1] = 'x';

    for (i = 0; i < 8; i++) {
        buffer[2 + i] = hex_chars[(value >> (28 - i * 4)) & 0xF];
    }
    buffer[10] = '\0';

    print_str(buffer);
}

void print_dec(uint32_t value) {
    char buffer[12];  // Máximo 10 dígitos + null
    int i = 10;

    buffer[11] = '\0';

    if (value == 0) {
        print_char('0');
        return;
    }

    while (value > 0 && i > 0) {
        buffer[i--] = '0' + (value % 10);
        value /= 10;
    }

    print_str(&buffer[i + 1]);
}

void print_hex64(uint64_t value) {
    static const char hex_chars[] = "0123456789ABCDEF";
    char buffer[19];  // "0x" + 16 digits + null = 19 bytes
    int i;

    buffer[0] = '0';
    buffer[1] = 'x';

    // Imprimir desde el nibble más significativo (bit 60-63)
    for (i = 0; i < 16; i++) {
        buffer[2 + i] = hex_chars[(value >> (60 - i * 4)) & 0xF];
    }
    buffer[18] = '\0';

    print_str(buffer);
}

void print_dec64(uint64_t value) {
    char buffer[22];  // Máximo 20 dígitos + null
    int i = 20;

    buffer[21] = '\0';

    if (value == 0) {
        print_char('0');
        return;
    }

    while (value > 0 && i > 0) {
        buffer[i--] = '0' + (value % 10);
        value /= 10;
    }

    print_str(&buffer[i + 1]);
}

void print_dec_signed(int32_t value) {
    if (value < 0) {
        print_char('-');
        // Convertir a positivo evitando overflow en INT32_MIN
        print_dec64((uint64_t)(-(int64_t)value));
    } else {
        print_dec((uint32_t)value);
    }
}

void print_dec64_signed(int64_t value) {
    if (value < 0) {
        print_char('-');
        // Convertir a positivo evitando overflow en INT64_MIN
        print_dec64((uint64_t)(-value));
    } else {
        print_dec64((uint64_t)value);
    }
}

/* ==========================================
 * Funciones de Consulta [3.6]
 * ========================================== */

void print_get_cursor(size_t* col, size_t* row) {
    memory_barrier();
    if (col != nullptr) {
        *col = cursor_col;
    }
    if (row != nullptr) {
        *row = cursor_row;
    }
    memory_barrier();
}

void print_get_color(uint8_t* fg, uint8_t* bg) {
    memory_barrier();
    if (fg != nullptr) {
        *fg = current_color & 0x0F;  // Extraer foreground (bits 0-3)
    }
    if (bg != nullptr) {
        *bg = (current_color >> 4) & 0x0F;  // Extraer background (bits 4-7)
    }
    memory_barrier();
}

void print_set_cursor(size_t col, size_t row) {
    // Validar límites antes de establecer
    if (col >= VGA_COLS) {
        col = VGA_COLS - 1;
    }
    if (row >= VGA_ROWS) {
        row = VGA_ROWS - 1;
    }
    
    memory_barrier();
    cursor_col = col;
    cursor_row = row;
    memory_barrier();
}
