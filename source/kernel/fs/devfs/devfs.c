/**
 * Device management file system
 */
#include "dev/dev.h"
#include "fs/devfs/devfs.h"
#include "fs/fs.h"
#include "tools/klib.h"
#include "tools/log.h"
#include "fs/file.h"

// supported devices in device management file system (3 type)
static devfs_type_t devfs_type_list[] = {
    {
        .name = "tty",
        .dev_type = DEV_TTY,
        .file_type = FILE_TTY,
    }
};
/**
 * mount specific device
 * here don't need to consider about the major and minor
 */
int devfs_mount (struct _fs_t * fs, int major, int minor) {
    fs->type = FS_DEVFS;
    return 0;
}

/**
 * unmount specific device
 */
void devfs_unmount (struct _fs_t * fs) {
}

/**
 * open specific device to write or read
 */
int devfs_open (struct _fs_t * fs, const char * path, file_t * file) {   
    // iterarte all supported device list, based on path, find the corresponding device type
    for (int i = 0; i < sizeof(devfs_type_list) / sizeof(devfs_type_list[0]); i++) {
        devfs_type_t * type = devfs_type_list + i;

        // find the name and convert it to string
        int type_name_len = kernel_strlen(type->name);

        // if the path to mount is existed, get the child log
        if (kernel_strncmp(path, type->name, type_name_len) == 0) {
            int minor;

            // get the minor number
            if ((kernel_strlen(path) > type_name_len) && (path_to_num(path + type_name_len, &minor)) < 0) {
                log_printf("Get device num failed. %s", path);
                break;
            }

            // open the device
            int dev_id = dev_open(type->dev_type, minor, (void *)0);
            if (dev_id < 0) {
                log_printf("Open device failed:%s", path);
                break;
            }

            // store the device number
            file->dev_id = dev_id;
            file->fs = fs;
            file->pos = 0;
            file->size = 0;
            file->type = type->file_type;
            return 0;
        }
    }

    return -1;
}

/**
 * @brief read specific file system
 */
int devfs_read (char * buf, int size, file_t * file) {
    return dev_read(file->dev_id, file->pos, buf, size);
}

/**
 * @brief read specific file system
 */
int devfs_write (char * buf, int size, file_t * file) {
    return dev_write(file->dev_id, file->pos, buf, size);
}

/**
 * @brief close device file
 */
void devfs_close (file_t * file) {
    dev_close(file->dev_id);
}

/**
 * @brief get the location in file I/O
 */
int devfs_seek (file_t * file, uint32_t offset, int dir) {
    return -1;  // for now not support
}

/**
 * @brief get file info
 */
int devfs_stat(file_t * file, struct stat *st) {
    return -1;
}

/**
 * @brief I/O device control
 */
int devfs_ioctl(file_t * file, int cmd, int arg0, int arg1) {
    return dev_control(file->dev_id, cmd, arg0, arg1);
}

// file system operations
fs_op_t devfs_op = {
    .mount = devfs_mount,
    .unmount = devfs_unmount,
    .open = devfs_open,
    .read = devfs_read,
    .write = devfs_write,
    .seek = devfs_seek,
    .stat = devfs_stat,
    .close = devfs_close,
    .ioctl = devfs_ioctl,
};
