#ifndef STRING_H
#define STRING_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Copy memory region with overlap handling
 * @param dest Destination pointer
 * @param src Source pointer
 * @param n Number of bytes to copy
 * @return Pointer to dest, or NULL if dest is NULL
 *
 * NULL pointer handling standardized.
 *   - If dest is NULL: returns NULL (no-op)
 *   - If src is NULL: returns dest (no-op, avoids crash)
 *   - This allows chaining: memmove(a,b,n) = memmove(c,d,m)
 */
void* memmove(void* dest, const void* src, size_t n);

/**
 * @brief Copy memory region (non-overlapping)
 * @param dest Destination pointer
 * @param src Source pointer
 * @param n Number of bytes to copy
 * @return Pointer to dest, or NULL if dest is NULL
 *
 * NULL pointer handling standardized.
 *   - If dest is NULL: returns NULL (no-op)
 *   - If src is NULL: returns dest (no-op, avoids crash)
 *   - This allows chaining: memcpy(a,b,n) = memcpy(c,d,m)
 */
void* memcpy(void* dest, const void* src, size_t n);

/**
 * @brief Set memory region to a byte value
 * @param s Pointer to memory region
 * @param c Byte value to set
 * @param n Number of bytes to set
 * @return Pointer to s, or NULL if s is NULL
 *
 * NULL pointer handling standardized.
 *   - If s is NULL: returns NULL (no-op)
 */
void* memset(void* s, int c, size_t n);

/**
 * @brief Compare two memory regions
 * @param s1 First memory region
 * @param s2 Second memory region
 * @param n Number of bytes to compare
 * @return 0 if equal or if either pointer is NULL, <0 if s1<s2, >0 if s1>s2
 *
 * NULL pointer handling standardized.
 *   - If s1 or s2 is NULL: returns 0 (considered equal, no-op)
 *   - This prevents crashes but caller should validate pointers
 */
int memcmp(const void* s1, const void* s2, size_t n);

/**
 * @brief Copy string with size limit (SAFE bounded copy)
 * @param dest Destination buffer
 * @param src Source null-terminated string
 * @param destsize Size of destination buffer in bytes
 * @return Length of source string, or 0 if dest or src is NULL
 *
 * NULL pointer handling standardized.
 *   - If dest is NULL: returns 0 (no-op)
 *   - If src is NULL: returns 0 (no-op)
 *
 * Safe bounded string copy that prevents buffer overflow.
 *
 * Guarantees:
 *   - Never writes more than destsize bytes (including null terminator)
 *   - Always null-terminates if destsize > 0
 *   - Returns length of source (to detect truncation)
 *
 * @note If return value >= destsize, truncation occurred
 * @note If return value < destsize, copy was complete
 *
 * SECURITY NOTE: This is the ONLY safe string copy function.
 * The deprecated strcpy() has been removed to prevent buffer overflows.
 */
size_t strlcpy(char* dest, const char* src, size_t destsize);

/**
 * @brief Calculate string length (excluding null terminator)
 * @param str Null-terminated string to measure
 * @return Length of string, or 0 if str is NULL
 *
 * NULL pointer handling documented.
 *   - If str is NULL: returns 0 (safe handling)
 *
 * @note Returns 0 for empty string ("")
 * @note Does not include null terminator in count
 */
size_t strlen(const char* str);

#ifdef __cplusplus
}
#endif

#endif /* STRING_H */
