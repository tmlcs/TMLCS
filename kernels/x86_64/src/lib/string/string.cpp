#include "string.h"

/* ==========================================
 * DEBUG: Overlap detection for memcpy
 * ==========================================
 * Two memory regions [dest, dest+n) and [src, src+n) overlap if:
 *   dest < src + n  AND  src < dest + n
 *
 * This is equivalent to checking if the regions intersect.
 * ========================================== */
#ifdef DEBUG_ENABLE
#include "debug.h"

static inline bool memory_regions_overlap(const void* dest, const void* src, size_t n) {
    /* If n is 0, no overlap possible */
    if (n == 0) {
        return false;
    }

    /* Cast to uintptr_t for pointer arithmetic */
    uintptr_t d = reinterpret_cast<uintptr_t>(dest);
    uintptr_t s = reinterpret_cast<uintptr_t>(src);

    /* Check overlap condition: dest < src + n && src < dest + n */
    return (d < s + n) && (s < d + n);
}
#endif /* DEBUG_ENABLE */

/* ==========================================
 * memmove() - Copy memory with overlap handling
 * ==========================================
 * Unlike memcpy(), memmove() handles overlapping regions correctly
 * by copying to a temporary buffer if needed.
 *
 * @param dest Destination pointer
 * @param src Source pointer
 * @param n Number of bytes to copy
 * @return Pointer to dest
 *
 * @note Added NULL pointer validation
 *       Returns dest if dest is NULL (no-op)
 *       Returns dest if src is NULL (no-op, avoids crash)
 */
void* memmove(void* dest, const void* src, size_t n) {
    /* NULL pointer validation */
    if (dest == nullptr || src == nullptr) {
        return dest; /* No-op for NULL pointers, avoids triple fault */
    }

    uint8_t* d = static_cast<uint8_t*>(dest);
    const uint8_t* s = static_cast<const uint8_t*>(src);

    if (d < s) {
        // No overlap or dest before src: copy forward
        while (n--) {
            *d++ = *s++;
        }
    } else if (d > s) {
        // Overlap with dest after src: copy backward
        d += n;
        s += n;
        while (n--) {
            *--d = *--s;
        }
    }
    // If d == s, nothing to copy

    return dest;
}

/* ==========================================
 * memcpy() - Copy memory (no overlap)
 * ==========================================
 * Standard memcpy for non-overlapping regions.
 *
 * IMPORTANT: This function does NOT handle overlapping regions.
 * If overlap is possible, use memmove() instead.
 *
 * @param dest Destination pointer
 * @param src Source pointer
 * @param n Number of bytes to copy
 * @return Pointer to dest
 *
 * @note Overlap detection: Two regions [dest, dest+n) and [src, src+n)
 *       overlap if: dest < src + n && src < dest + n
 *
 * @note Added NULL pointer validation
 *       Returns dest if dest or src is NULL (avoids triple fault)
 *
 * @note DEBUG_ENABLE: Runtime overlap check with error report
 *       Release: No overhead, caller must ensure no overlap
 */
void* memcpy(void* dest, const void* src, size_t n) {
    /* NULL pointer validation */
    if (dest == nullptr || src == nullptr) {
        return dest; /* No-op for NULL pointers, avoids triple fault */
    }

    uint8_t* d = static_cast<uint8_t*>(dest);
    const uint8_t* s = static_cast<const uint8_t*>(src);

/* ==========================================
 * Overlap detection in DEBUG mode
 * ==========================================
 * If overlap is detected, report error via DEBUG_PRINT
 * and fall back to memmove() to prevent corruption.
 * ========================================== */
#ifdef DEBUG_ENABLE
    if (memory_regions_overlap(dest, src, n)) {
        DEBUG_PRINT("\r\n[MEMCPY OVERLAP DETECTED]\r\n");
        DEBUG_PRINT("  Using memmove() instead for safety\r\n");
        DEBUG_PRINT("  src:  0x");
        DEBUG_PRINT_HEX((uintptr_t) src);
        DEBUG_PRINT("\r\n  dest: 0x");
        DEBUG_PRINT_HEX((uintptr_t) dest);
        DEBUG_PRINT("\r\n  size: ");
        DEBUG_PRINT_DEC(n);
        DEBUG_PRINT("\r\n");

        /* Use memmove for overlapping regions */
        return memmove(dest, src, n);
    }
#endif /* DEBUG_ENABLE */

    /* Non-overlapping copy: simple forward copy */
    while (n--) {
        *d++ = *s++;
    }

    return dest;
}

/* ==========================================
 * memset() - Set memory to a value
 * ==========================================
 * Fill a memory region with a byte value.
 *
 * @param s Pointer to memory region
 * @param c Byte value to set
 * @param n Number of bytes to set
 * @return Pointer to s
 *
 * @note Added NULL pointer validation
 *       Returns s if s is NULL (no-op, avoids triple fault)
 */
void* memset(void* s, int c, size_t n) {
    /* NULL pointer validation */
    if (s == nullptr) {
        return s; /* No-op for NULL pointer, avoids triple fault */
    }

    uint8_t* p = static_cast<uint8_t*>(s);

    while (n--) {
        *p++ = static_cast<uint8_t>(c);
    }

    return s;
}

/* ==========================================
 * memcmp() - Compare memory regions
 * ==========================================
 * Compare two memory regions byte by byte.
 *
 * @param s1 First memory region
 * @param s2 Second memory region
 * @param n Number of bytes to compare
 * @return 0 if equal, <0 if s1<s2, >0 if s1>s2
 *
 * @note Added NULL pointer validation
 *       If either pointer is NULL, returns 0 (equal, no-op)
 */
int memcmp(const void* s1, const void* s2, size_t n) {
    /* NULL pointer validation */
    if (s1 == nullptr || s2 == nullptr) {
        return 0; /* Consider NULL pointers as equal, avoids triple fault */
    }

    const uint8_t* p1 = static_cast<const uint8_t*>(s1);
    const uint8_t* p2 = static_cast<const uint8_t*>(s2);

    while (n--) {
        int diff = *p1++ - *p2++;
        if (diff != 0) {
            return diff;
        }
    }

    return 0;
}

/* ==========================================
 * strcpy() - Copy string with null terminator
 * ==========================================
 * Copies the null-terminated string from src to dest.
 *
 * @param dest Destination buffer (must be large enough)
 * @param src Source null-terminated string
 * @return Pointer to dest
 *
 * @warning UNSAFE - No bounds checking. Use strlcpy() instead.
 * @warning Destination buffer must be large enough to hold the source string
 *          including the null terminator.
 *
 * @note Added NULL pointer validation
 *       Returns dest if dest or src is NULL (avoids triple fault)
 *
 * @note Copies characters including null terminator
 * @note Returns dest for chaining compatibility
 */
char* strcpy(char* dest, const char* src) {
    /* NULL pointer validation */
    if (dest == nullptr || src == nullptr) {
        return dest; /* No-op for NULL pointers, avoids triple fault */
    }

    char* original_dest = dest;

    // Copy characters including null terminator
    while ((*dest++ = *src++) != '\0') {
        // Empty body - copy happens in condition
    }

    return original_dest;
}

/* ==========================================
 * strlcpy() - Copy string with size limit
 * ==========================================
 * Safe bounded string copy that prevents buffer overflow.
 *
 * @param dest Destination buffer
 * @param src Source null-terminated string
 * @param destsize Size of destination buffer in bytes
 * @return Length of source string (not including null terminator)
 *
 * This function guarantees:
 *   1. Never writes more than destsize bytes (including null terminator)
 *   2. Always null-terminates if destsize > 0
 *   3. Returns length of source (to detect truncation)
 *
 * Truncation detection:
 *   - If return value >= destsize: truncation occurred
 *   - If return value < destsize: copy was complete
 *
 * @note Also validates NULL pointers for safety
 *
 * @example
 *     // Truncation case
 *     char src[] = "Hello, World!";
 *     char dest[6];
 *     size_t len = strlcpy(dest, src, sizeof(dest));
 *     // Result: dest = "Hello\0", len = 13
 *     // Truncation detected: len (13) >= destsize (6)
 *
 * @example
 *     // Complete copy case
 *     char src[] = "Hi";
 *     char dest[10];
 *     size_t len = strlcpy(dest, src, sizeof(dest));
 *     // Result: dest = "Hi\0", len = 2
 *     // No truncation: len (2) < destsize (10)
 */
size_t strlcpy(char* dest, const char* src, size_t destsize) {
    /* NULL pointer validation */
    if (dest == nullptr || src == nullptr) {
        return 0; /* Cannot copy, return 0 */
    }

    /* Handle zero-size buffer: just calculate source length */
    if (destsize == 0) {
        return strlen(src);
    }

    const char* original_src = src;
    size_t i = 0;

    /* Copy at most destsize - 1 characters, leaving room for null terminator */
    for (i = 0; i < destsize - 1 && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }

    /* Always null-terminate (if destsize > 0) */
    dest[i] = '\0';

    /* Return length of source string (for truncation detection) */
    return strlen(original_src);
}

/* ==========================================
 * strlen() - Calculate string length
 * ==========================================
 * Returns the length of a null-terminated string.
 *
 * @param str Null-terminated string to measure
 * @return Number of characters before null terminator
 *
 * @note Returns 0 for empty string ("")
 * @note Does not include null terminator in count
 * @note Safe: handles null pointer by returning 0
 */
size_t strlen(const char* str) {
    if (str == nullptr) {
        return 0;  // Safe handling of null pointer
    }

    size_t len = 0;
    while (str[len] != '\0') {
        len++;
    }

    return len;
}
