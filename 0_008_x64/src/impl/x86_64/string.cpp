#include "string.h"

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
 */
void* memmove(void* dest, const void* src, size_t n) {
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
 */
void* memcpy(void* dest, const void* src, size_t n) {
    uint8_t* d = static_cast<uint8_t*>(dest);
    const uint8_t* s = static_cast<const uint8_t*>(src);

    // Note: Runtime overlap check removed to avoid dependency on debug.h
    // which causes circular dependencies with serial.h/print.h.
    // Callers must ensure regions do not overlap.
    // Use memmove() for overlapping regions.

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
 */
void* memset(void* s, int c, size_t n) {
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
 */
int memcmp(const void* s1, const void* s2, size_t n) {
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
