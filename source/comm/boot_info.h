/**
 * System boot info
 *
 */
#ifndef BOOT_INFO_H
#define BOOT_INFO_H

#include "types.h"

#define BOOT_RAM_REGION_MAX			10		// RAM Max number

/**
 * Boot info
 */
typedef struct _boot_info_t {
    // RAM info
    struct {
        uint32_t start;
        uint32_t size;
    }ram_region_cfg[BOOT_RAM_REGION_MAX];
    int ram_region_count;
}boot_info_t;

#define SECTOR_SIZE		512			// disk sector size
#define SYS_KERNEL_LOAD_ADDR		(1024*1024)		// start address kernel load

#endif // BOOT_INFO_H
