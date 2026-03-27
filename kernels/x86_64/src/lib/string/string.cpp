#include "string.h"
#include "panic.h"

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
 * @return Pointer to dest, or NULL if dest is NULL
 *
 * NULL pointer handling standardized.
 *   - If dest is NULL: returns NULL (no-op, avoids triple fault)
 *   - If src is NULL: returns dest (no-op, avoids crash)
 *   - Behavior is consistent: always return first parameter (dest)
 *   - This allows chaining: memmove(a,b,n) = memmove(c,d,m)
 */
void* memmove(void* dest, const void* src, size_t n) {
    /* NULL pointer validation */
    if (dest == nullptr || src == nullptr) {
        return dest; /* Return dest for consistency */
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
 * @return Pointer to dest, or NULL if dest is NULL
 *
 * NULL pointer handling standardized.
 *   - If dest is NULL: returns NULL (no-op, avoids triple fault)
 *   - If src is NULL: returns dest (no-op, avoids crash)
 *   - Behavior is consistent: always return first parameter (dest)
 *   - This allows chaining: memcpy(a,b,n) = memcpy(c,d,m)
 *
 * @note DEBUG_ENABLE: Runtime overlap check with error report
 *       Release: No overhead, caller must ensure no overlap
 */
void* memcpy(void* dest, const void* src, size_t n) {
    /* NULL pointer validation */
    if (dest == nullptr || src == nullptr) {
        return dest; /* Return dest for consistency */
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
 * @return Pointer to s, or NULL if s is NULL
 *
 * NULL pointer handling standardized.
 *   - If s is NULL: returns NULL (no-op, avoids triple fault)
 *   - Behavior is consistent: always return first parameter (s)
 */
void* memset(void* s, int c, size_t n) {
    /* NULL pointer validation */
    if (s == nullptr) {
        return s; /* Return s for consistency */
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
 * NULL pointer handling: if either pointer is NULL, returns 0.
 *   This satisfies the header contract without panicking the kernel.
 */
int memcmp(const void* s1, const void* s2, size_t n) {
    if (s1 == nullptr || s2 == nullptr) {
        return 0;
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
 * strlcpy() - Copy string with size limit
 * ==========================================
 * Safe bounded string copy that prevents buffer overflow.
 *
 * @param dest Destination buffer
 * @param src Source null-terminated string
 * @param destsize Size of destination buffer in bytes
 * @return Length of source string, or 0 if dest or src is NULL
 *
 * NULL pointer handling standardized.
 *   - If dest is NULL: returns 0 (no-op)
 *   - If src is NULL: returns 0 (no-op)
 *   - If destsize is 0: returns strlen(src) (truncation detection)
 *
 * This function guarantees:
 *   1. Never writes more than destsize bytes (including null terminator)
 *   2. Always null-terminates if destsize > 0
 *   3. Returns length of source (to detect truncation)
 *
 * Truncation detection:
 *   - If return value >= destsize: truncation occurred
 *   - If return value < destsize: copy was complete
 */
size_t strlcpy(char* dest, const char* src, size_t destsize) {
    /* NULL pointer validation */
    if (dest == nullptr || src == nullptr) {
        return 0; /* Cannot copy, return 0 */
    }

    /* LOW-NEW-009 FIX: Single-pass implementation.
     * Previously: copy loop (up to destsize-1 chars) + strlen(original_src)
     * on the full source string — two passes for the common non-truncated case.
     * Now: one unified loop copies up to destsize-1 chars while advancing src;
     * after the copy the remaining tail of src (if any) is counted inline.
     * In the common non-truncated case src[i] == '\0' immediately ends both
     * phases, so the string is traversed exactly once. */
    size_t i = 0;

    if (destsize > 0) {
        /* Copy at most destsize - 1 characters */
        while (i + 1 < destsize && src[i] != '\0') {
            dest[i] = src[i];
            i++;
        }
        dest[i] = '\0';
    }

    /* Count any remaining source characters to return full source length */
    size_t src_len = i;
    while (src[src_len] != '\0') {
        src_len++;
    }
    return src_len;
}

/* ==========================================
 * strlen() - Calculate string length
 * ==========================================
 * Returns the length of a null-terminated string.
 *
 * @param str Null-terminated string to measure
 * @return Length of string, or 0 if str is NULL
 *
 * NULL pointer handling documented.
 *   - If str is NULL: returns 0 (safe handling, avoids crash)
 *
 * @note Returns 0 for empty string ("")
 * @note Does not include null terminator in count
 */
size_t strlen(const char* str) {
    /* NULL pointer handling */
    if (str == nullptr) {
        return 0; /* Safe handling of null pointer */
    }

    size_t len = 0;
    while (str[len] != '\0') {
        len++;
    }

    return len;
}
