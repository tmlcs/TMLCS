#ifndef STRING_H
#define STRING_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Copy memory region with overlap handling
 * @param dest Destination pointer
 * @param src Source pointer
 * @param n Number of bytes to copy
 * @return Pointer to dest
 */
void* memmove(void* dest, const void* src, size_t n);

/**
 * @brief Copy memory region (non-overlapping)
 * @param dest Destination pointer
 * @param src Source pointer
 * @param n Number of bytes to copy
 * @return Pointer to dest
 */
void* memcpy(void* dest, const void* src, size_t n);

/**
 * @brief Set memory region to a byte value
 * @param s Pointer to memory region
 * @param c Byte value to set
 * @param n Number of bytes to set
 * @return Pointer to s
 */
void* memset(void* s, int c, size_t n);

/**
 * @brief Compare two memory regions
 * @param s1 First memory region
 * @param s2 Second memory region
 * @param n Number of bytes to compare
 * @return 0 if equal, <0 if s1<s2, >0 if s1>s2
 */
int memcmp(const void* s1, const void* s2, size_t n);

/**
 * @brief Copy string with null terminator
 * @param dest Destination buffer (must be large enough)
 * @param src Source null-terminated string
 * @return Pointer to dest
 * 
 * @warning Destination buffer must be large enough to hold the source string
 *          including the null terminator. No bounds checking is performed.
 * 
 * @example
 *     char src[] = "Hello";
 *     char dest[6];  // 5 chars + null terminator
 *     strcpy(dest, src);  // OK
 */
char* strcpy(char* dest, const char* src);

/**
 * @brief Calculate string length (excluding null terminator)
 * @param str Null-terminated string to measure
 * @return Number of characters before null terminator
 * 
 * @note Returns 0 for empty string ("")
 * @note Does not include null terminator in count
 * 
 * @example
 *     strlen("Hello")     // Returns: 5
 *     strlen("")          // Returns: 0
 *     strlen("ABC")       // Returns: 3
 */
size_t strlen(const char* str);

#ifdef __cplusplus
}
#endif

#endif /* STRING_H */
