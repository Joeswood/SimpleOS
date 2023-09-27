/**
 * Bitmap data structure
 */
#include "tools/bitmap.h"
#include "tools/klib.h"

/**
 * @brief Retrieve the required number of bytes
 */
int bitmap_byte_count (int bit_count) {
    return (bit_count + 8 - 1) / 8;         // round up
}

/**
 * @brief Init bitmap
 */
void bitmap_init (bitmap_t * bitmap, uint8_t * bits, int count, int init_bit) {
    bitmap->bit_count = count;
    bitmap->bits = bits;

    int bytes = bitmap_byte_count(bitmap->bit_count);
    kernel_memset(bitmap->bits, init_bit ? 0xFF: 0, bytes);
}

/**
 * @brief Set N bit
 */
void bitmap_set_bit (bitmap_t * bitmap, int index, int count, int bit) {
    for (int i = 0; (i < count) && (index < bitmap->bit_count); i++, index++) {
        if (bit) {
            bitmap->bits[index / 8] |= 1 << (index % 8);
        } else {
            bitmap->bits[index / 8] &= ~(1 << (index % 8));
        }
    }
} 

/**
 * @brief Retrieve the state of a specified bit
 */
int bitmap_get_bit (bitmap_t * bitmap, int index) {
    //return bitmap->bits[index / 8] & (1 << (index % 8));
    return (bitmap->bits[index / 8] & (1 << (index % 8))) ? 1 : 0;
}

/**
 * @brief Check if a specified bit is set to 1
 */
int bitmap_is_set (bitmap_t * bitmap, int index) {
    return bitmap_get_bit(bitmap, index) ? 1 : 0;
}

/**
 * @brief Allocate consecutive specified bit positions and return the starting index
 */
int bitmap_alloc_nbits (bitmap_t * bitmap, int bit, int count) {
    int search_idx = 0;
    int ok_idx = -1;

    while (search_idx < bitmap->bit_count) {
        // navigate to the first matching index
        if (bitmap_get_bit(bitmap, search_idx) != bit) {
            // if different, keep looking for the initial bit
            search_idx++;
            continue;
        }

        // record start index
        ok_idx = search_idx;

        // continue to calcuate next part
        int i;
        for (i = 1; (i < count) && (search_idx < bitmap->bit_count); i++) {
            if (bitmap_get_bit(bitmap, search_idx++) != bit) {
                // not enough, quit
                ok_idx = -1;
                break;
            }
        }

        // found, set bits and quit
        if (i >= count) {
            bitmap_set_bit(bitmap, ok_idx, count, !bit);
            return ok_idx;
        }
    }

    return -1;
}

