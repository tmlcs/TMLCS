#ifndef PANIC_H
#define PANIC_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================
 * Kernel Panic - Error fatal del sistema
 * ==========================================
 * Cuando ocurre un error del que no se puede recuperar,
 * el kernel debe:
 * 1. Imprimir mensaje de error (VGA y serial)
 * 2. Detener la ejecución de forma segura
 * 3. Nunca retornar
 */

/**
 * @brief Kernel panic - error fatal del que no se puede recuperar
 * @param message Mensaje de error descriptivo
 * @param error_code Código de error opcional (0 si no aplica)
 * 
 * @note Esta función NUNCA retorna. Detiene el kernel indefinidamente.
 * @note Imprime en VGA (si está disponible) y serial (si está inicializado)
 */
void panic(const char* message, uint32_t error_code);

/**
 * @brief Kernel panic con mensaje simple (sin código de error)
 * @param message Mensaje de error descriptivo
 */
void panic_simple(const char* message);

/**
 * @brief Verificar condición y hacer panic si es falsa
 * @param condition Condición que debe ser verdadera
 * @param message Mensaje de error si la condición es falsa
 * 
 * @note Macro para conveniencia - se expande a panic_simple() si falla
 */
#define PANIC_IF_FALSE(condition, message) \
    do { \
        if (!(condition)) { \
            panic_simple(message); \
        } \
    } while(0)

#ifdef __cplusplus
}
#endif

#endif /* PANIC_H */
