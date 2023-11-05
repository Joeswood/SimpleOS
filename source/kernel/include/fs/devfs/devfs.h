/**
 * Device File System
 */
#ifndef DEVFS_H
#define DEVFS_H

#include "fs/fs.h"

/**
 * @brief Device type descriptor
 */
typedef struct _devfs_type_t {
    const char * name;
    int dev_type;
    int file_type;
}devfs_type_t;

#endif
