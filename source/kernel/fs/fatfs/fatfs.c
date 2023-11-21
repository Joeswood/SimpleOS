/**
 * Simple FAT File System
 */
#include "fs/fs.h"
#include "fs/fatfs/fatfs.h"
#include "dev/dev.h"
#include "core/memory.h"
#include "tools/log.h"
#include "tools/klib.h"
#include <sys/fcntl.h>

/**
 * @brief Buffer reads disk data
 */
static int bread_sector (fat_t * fat, int sector) {
    if (sector == fat->curr_sector) {
        return 0;
    }

    int cnt = dev_read(fat->fs->dev_id, sector, fat->fat_buffer, 1);
    if (cnt == 1) {
        fat->curr_sector = sector;
        return 0;
    }
    return -1;
}

/**
 * @brief Write buffer
 */
static int bwrite_secotr (fat_t * fat, int sector) {
    int cnt = dev_write(fat->fs->dev_id, sector, fat->fat_buffer, 1);
    return (cnt == 1) ? 0 : -1;
}

/**
 * @brief Check if cluster is valid
 */
int cluster_is_valid (cluster_t cluster) {
    return (cluster < 0xFFF8) && (cluster >= 0x2);     
}

/**
 * @brief Retrieve specific cluster
 */
int cluster_get_next (fat_t * fat, cluster_t curr) {
    if (!cluster_is_valid(curr)) {
        return FAT_CLUSTER_INVALID;
    }

    // retrieve offset, sector number in fat table 
    int offset = curr * sizeof(cluster_t);
    int sector = offset / fat->bytes_per_sec;
    int off_sector = offset % fat->bytes_per_sec;
    if (sector >= fat->tbl_sectors) {
        log_printf("cluster too big. %d", curr);
        return FAT_CLUSTER_INVALID;
    }

    // read sector and get cluster data
    int err = bread_sector(fat, fat->tbl_start + sector);
    if (err < 0) {
        return FAT_CLUSTER_INVALID;
    }

    return *(cluster_t*)(fat->fat_buffer + off_sector);
}

/**
 * @brief Set next cluster
 */
int cluster_set_next (fat_t * fat, cluster_t curr, cluster_t next) {
    if (!cluster_is_valid(curr)) {
        return -1;
    }

    int offset = curr * sizeof(cluster_t);
    int sector = offset / fat->bytes_per_sec;
    int off_sector = offset % fat->bytes_per_sec;
    if (sector >= fat->tbl_sectors) {
        log_printf("cluster too big. %d", curr);
        return -1;
    }

    // Read buffer
    int err = bread_sector(fat, fat->tbl_start + sector);
    if (err < 0) {
        return -1;
    }

    // Change next
    *(cluster_t*)(fat->fat_buffer + off_sector) = next;

    // Write back to multi tables
    for (int i = 0; i < fat->tbl_cnt; i++) {
        err = bwrite_secotr(fat, fat->tbl_start + sector);
        if (err < 0) {
            log_printf("write cluster failed.");
            return -1;
        }
        sector += fat->tbl_sectors;
    }
    return 0;
}

/**
 * @brief Release cluster chain
 */
void cluster_free_chain(fat_t * fat, cluster_t start) {
    while (cluster_is_valid(start)) {
        cluster_t next = cluster_get_next(fat, start);
        cluster_set_next(fat, start, FAT_CLUSTER_FREE);
        start = next;
    }
}

/**
 * @brief Finde a idle cluster
 */
cluster_t cluster_alloc_free (fat_t * fat, int cnt) {
    cluster_t pre, curr, start;
    int c_total = fat->tbl_sectors * fat->bytes_per_sec / sizeof(cluster_t);

    pre = start = FAT_CLUSTER_INVALID;
    for (curr = 2; (curr< c_total) && cnt; curr++) {
        cluster_t free = cluster_get_next(fat, curr);
        if (free == FAT_CLUSTER_FREE) {
            // record first cluster
            if (!cluster_is_valid(start)) {
                start = curr;
            } 
        
            // if previous cluster is valid, set otherwise ignore
            if (cluster_is_valid(pre)) {
                // find empty table entry, set previous link
                int err = cluster_set_next(fat, pre, curr);
                if (err < 0) {
                    cluster_free_chain(fat, start);
                    return FAT_CLUSTER_INVALID;
                }
            }

            pre = curr;
            cnt--;
        }
    }

    // final node
    if (cnt == 0) {
        int err = cluster_set_next(fat, pre, FAT_CLUSTER_INVALID);
        if (err == 0) {
            return start;
        }
    }

    //fail(no available space)
    cluster_free_chain(fat, start);
    return FAT_CLUSTER_INVALID;
}

/**
 * @brief Convert file name into short name
 */
static void to_sfn(char* dest, const char* src) {
    kernel_memset(dest, ' ', SFN_LEN);

    // generate until meet ‘ ’ and write buffer
    char * curr = dest;
    char * end = dest + SFN_LEN;
    while (*src && (curr < end)) {
        char c = *src++;

        switch (c) {
        case '.':     
            curr = dest + 8;
            break;
        default:
            if ((c >= 'a') && (c <= 'z')) {
                c = c - 'a' + 'A';
            }
            *curr++ = c;
            break;
        }
    }
}

/**
 * @brief Check if the item matches the specified name
 */
int diritem_name_match (diritem_t * item, const char * path) {
    char buf[SFN_LEN];
    to_sfn(buf, path);
    return kernel_memcmp(buf, item->DIR_Name, SFN_LEN) == 0;
}

/**
 * Initialize driitem with default values
 */
int diritem_init(diritem_t * item, uint8_t attr,const char * name) {
    to_sfn((char *)item->DIR_Name, name);
    item->DIR_FstClusHI = (uint16_t )(FAT_CLUSTER_INVALID >> 16);
    item->DIR_FstClusL0 = (uint16_t )(FAT_CLUSTER_INVALID & 0xFFFF);
    item->DIR_FileSize = 0;
    item->DIR_Attr = attr;
    item->DIR_NTRes = 0;

    // Set a fixed value for the time
    item->DIR_CrtTime = 0;
    item->DIR_CrtDate = 0;
    item->DIR_WrtTime = item->DIR_CrtTime;
    item->DIR_WrtDate = item->DIR_CrtDate;
    item->DIR_LastAccDate = item->DIR_CrtDate;
    return 0;
}

/**
 * @brief Retrieve the name from diritem and convert it as needed
 */
void diritem_get_name (diritem_t * item, char * dest) {
    char * c = dest;
    char * ext = (char *)0;

    kernel_memset(dest, 0, SFN_LEN + 1);     // Max 11 char
    for (int i = 0; i < 11; i++) {
        if (item->DIR_Name[i] != ' ') {
            *c++ = item->DIR_Name[i];
        }

        if (i == 7) {
            ext = c;
            *c++ = '.';
        }
    }

    // without an extension
    if (ext && (ext[1] == '\0')) {
        ext[0] = '\0';
    }
}

/**
 * @brief Retrieve file type
 */
file_type_t diritem_get_type (diritem_t * item) {
    file_type_t type = FILE_UNKNOWN;

    // long file name and volum id
    if (item->DIR_Attr & (DIRITEM_ATTR_VOLUME_ID | DIRITEM_ATTR_HIDDEN | DIRITEM_ATTR_SYSTEM)) {
        return FILE_UNKNOWN;
    }

    return item->DIR_Attr & DIRITEM_ATTR_DIRECTORY ? FILE_DIR : FILE_NORMAL;
}

/**
 * @brief Read diritem in root dir
 */
static diritem_t * read_dir_entry (fat_t * fat, int index) {
    if ((index < 0) || (index >= fat->root_ent_cnt)) {
        return (diritem_t *)0;
    }

    int offset = index * sizeof(diritem_t);
    int err = bread_sector(fat, fat->root_start + offset / fat->bytes_per_sec);
    if (err < 0) {
        return (diritem_t *)0;
    }
    return (diritem_t *)(fat->fat_buffer + offset % fat->bytes_per_sec);
}

/**
 * @brief Write dir entry
 */
static int write_dir_entry (fat_t * fat, diritem_t * item, int index) {
    if ((index < 0) || (index >= fat->root_ent_cnt)) {
        return -1;
    }

    int offset = index * sizeof(diritem_t);
    int sector = fat->root_start + offset / fat->bytes_per_sec;
    int err = bread_sector(fat, sector);
    if (err < 0) {
        return -1;
    }
    kernel_memcpy(fat->fat_buffer + offset % fat->bytes_per_sec, item, sizeof(diritem_t));
    return bwrite_secotr(fat, sector);
}


/**
 * @brief Change file size
 */
static int expand_file(file_t * file, int inc_bytes) {
    fat_t * fat = (fat_t *)file->fs->data;
    
    int cluster_cnt;
    if ((file->size == 0) || (file->size % fat->cluster_byte_size == 0)) {
        // empty file or end of the cluster
        cluster_cnt = up2(inc_bytes, fat->cluster_byte_size) / fat->cluster_byte_size; 
    } else {
        // not empty file, if space is enough just quit
        int cfree = fat->cluster_byte_size - (file->size % fat->cluster_byte_size);
        if (cfree >= inc_bytes) {
            file->size += inc_bytes;
            return 0;
        }

        // not enough, allocate new cluster to extra space
        cluster_cnt = up2(inc_bytes - cfree, fat->cluster_byte_size) / fat->cluster_byte_size; 
    }

    cluster_t start = cluster_alloc_free(fat, cluster_cnt);
    if (!cluster_is_valid(start)) {
        log_printf("no cluster for file write");
        return -1;
    }

    // when close the file, write back
    if (!cluster_is_valid(file->sblk)) {
        file->cblk = file->sblk = start;
    } else {
        // build link
        int err = cluster_set_next(fat, file->cblk, start);
        if (err < 0) {
            return -1;
        }
    }

    return 0;
}

/**
 * @brief Move file pointer
 */
static int move_file_pos(file_t* file, fat_t * fat, uint32_t move_bytes, int expand) {
	uint32_t c_offset = file->pos % fat->cluster_byte_size;

    // if it's a cluster, then adjust curr_cluster. Note that curr_cluster won't be adjusted if it's already the last one
	if (c_offset + move_bytes >= fat->cluster_byte_size) {
        cluster_t next = cluster_get_next(fat, file->cblk);
		if ((next == FAT_CLUSTER_INVALID) && expand) {
            int err = expand_file(file, fat->cluster_byte_size);
            if (err < 0) {
                return -1;
            }

            next = cluster_get_next(fat, file->cblk);
        }

        file->cblk = next;
	}

	file->pos += move_bytes;
	return 0;
}

/**
 * @brief Mount FAT File System
 */
int fatfs_mount (struct _fs_t * fs, int dev_major, int dev_minor) {
    // open device
    int dev_id = dev_open(dev_major, dev_minor, (void *)0);
    if (dev_id < 0) {
        log_printf("open disk failed. major: %x, minor: %x", dev_major, dev_minor);
        return -1;
    }

    // read dbr sector and check
    dbr_t * dbr = (dbr_t *)memory_alloc_page();
    if (!dbr) {
        log_printf("mount fat failed: can't alloc buf.");
        goto mount_failed;
    }

    int cnt = dev_read(dev_id, 0, (char *)dbr, 1);
    if (cnt < 1) {
        log_printf("read dbr failed.");
        goto mount_failed;
    }

    // parse DBR parameters
    fat_t * fat = &fs->fat_data;
    fat->fat_buffer = (uint8_t *)dbr;
    fat->bytes_per_sec = dbr->BPB_BytsPerSec;
    fat->tbl_start = dbr->BPB_RsvdSecCnt;
    fat->tbl_sectors = dbr->BPB_FATSz16;
    fat->tbl_cnt = dbr->BPB_NumFATs;
    fat->root_ent_cnt = dbr->BPB_RootEntCnt;
    fat->sec_per_cluster = dbr->BPB_SecPerClus;
    fat->cluster_byte_size = fat->sec_per_cluster * dbr->BPB_BytsPerSec;
	fat->root_start = fat->tbl_start + fat->tbl_sectors * fat->tbl_cnt;
    fat->data_start = fat->root_start + fat->root_ent_cnt * 32 / SECTOR_SIZE;
    fat->curr_sector = -1;
    fat->fs = fs;
    mutex_init(&fat->mutex);
    fs->mutex = &fat->mutex;

    // check if it's fat 16 file system
	if (fat->tbl_cnt != 2) {
        log_printf("fat table num error, major: %x, minor: %x", dev_major, dev_minor);
		goto mount_failed;
	}

    if (kernel_memcmp(dbr->BS_FileSysType, "FAT16", 5) != 0) {
        log_printf("not a fat16 file system, major: %x, minor: %x", dev_major, dev_minor);
        goto mount_failed;
    }

    // record open status
    fs->type = FS_FAT16;
    fs->data = &fs->fat_data;
    fs->dev_id = dev_id;
    return 0;

mount_failed:
    if (dbr) {
        memory_free_page((uint32_t)dbr);
    }
    dev_close(dev_id);
    return -1;
}

/**
 * @brief Unmount fatfs File System
 */
void fatfs_unmount (struct _fs_t * fs) {
    fat_t * fat = (fat_t *)fs->data;

    dev_close(fs->dev_id);
    memory_free_page((uint32_t)fat->fat_buffer);
}

/**
 * @brief Retrieve the relevant file information from the diritem
 */
static void read_from_diritem (fat_t * fat, file_t * file, diritem_t * item, int index) {
    file->type = diritem_get_type(item);
    file->size = (int)item->DIR_FileSize;
    file->pos = 0;
    file->sblk = (item->DIR_FstClusHI << 16) | item->DIR_FstClusL0;
    file->cblk = file->sblk;
    file->p_index = index;
}

/**
 * @brief Open specific file
 */
int fatfs_open (struct _fs_t * fs, const char * path, file_t * file) {
    fat_t * fat = (fat_t *)fs->data;
    diritem_t * file_item = (diritem_t *)0;
    int p_index = -1;

    // iterate root dir data, find exised matching entry
    for (int i = 0; i < fat->root_ent_cnt; i++) {
        diritem_t * item = read_dir_entry(fat, i);
        if (item == (diritem_t *)0) {
            return -1;
        }

         // end entry.
        if (item->DIR_Name[0] == DIRITEM_NAME_END) {
            p_index = i;
            break;
        }

        // only show normal file and dir
        if (item->DIR_Name[0] == DIRITEM_NAME_FREE) {
            p_index = i;
            continue;
        }

        // find specific dir
        if (diritem_name_match(item, path)) {
            file_item = item;
            p_index = i;
            break;
        }
    }

    if (file_item) {
        read_from_diritem(fat, file, file_item, p_index);

        if (file->mode & O_TRUNC) {
            cluster_free_chain(fat, file->sblk);
            file->cblk = file->sblk = FAT_CLUSTER_INVALID;
            file->size = 0;
        }
        return 0;
    } else if ((file->mode & O_CREAT) && (p_index >= 0)) {
        // create a idle diritem entry
        diritem_t item;
        diritem_init(&item, 0, path);
        int err = write_dir_entry(fat, &item, p_index);
        if (err < 0) {
            log_printf("create file failed.");
            return -1;
        }

        read_from_diritem(fat, file, &item, p_index);
        return 0;
    }

    return -1;
}

/**
 * @brief Read file
 */
int fatfs_read (char * buf, int size, file_t * file) {
    fat_t * fat = (fat_t *)file->fs->data;

    // modify how many need to read, not beyond capacity
    uint32_t nbytes = size;
    if (file->pos + nbytes > file->size) {
        nbytes = file->size - file->pos;
    }

    uint32_t total_read = 0;
    while (nbytes > 0) {
        uint32_t curr_read = nbytes;
		uint32_t cluster_offset = file->pos % fat->cluster_byte_size;
        uint32_t start_sector = fat->data_start + (file->cblk - 2)* fat->sec_per_cluster;  

        // if it's cluster, read only one cluster
        if ((cluster_offset == 0) && (nbytes == fat->cluster_byte_size)) {
            int err = dev_read(fat->fs->dev_id, start_sector, buf, fat->sec_per_cluster);
            if (err < 0) {
                return total_read;
            }

            curr_read = fat->cluster_byte_size;
        } else {
            // If it crosses clusters, only read a portion of the first cluster
            if (cluster_offset + curr_read > fat->cluster_byte_size) {
                curr_read = fat->cluster_byte_size - cluster_offset;
            }

            // Read the entire cluster and then copy from it
            fat->curr_sector = -1;
            int err = dev_read(fat->fs->dev_id, start_sector, fat->fat_buffer, fat->sec_per_cluster);
            if (err < 0) {
                return total_read;
            }
            kernel_memcpy(buf, fat->fat_buffer + cluster_offset, curr_read);
        }

        buf += curr_read;
        nbytes -= curr_read;
        total_read += curr_read;

        // Move the file pointer forward
		int err = move_file_pos(file, fat, curr_read, 0);
		if (err < 0) {
            return total_read;
        }
	}

    return total_read;
}

/**
 * @brief Write file
 */
int fatfs_write (char * buf, int size, file_t * file) {
    fat_t * fat = (fat_t *)file->fs->data;

    // If the file size is not sufficient, first expand the file size
    if (file->pos + size > file->size) {
        int inc_size = file->pos + size - file->size;
        int err = expand_file(file, inc_size);
        if (err < 0) {
            return 0;
        }
    }

    uint32_t nbytes = size;
    uint32_t total_write = 0;
	while (nbytes) {
        // The amount of data written each time depends on the remaining space in the current cluster, as well as the overall size
        uint32_t curr_write = nbytes;
		uint32_t cluster_offset = file->pos % fat->cluster_byte_size;
        uint32_t start_sector = fat->data_start + (file->cblk - 2)* fat->sec_per_cluster;  // 从2开始

        // If it's a whole cluster, write the entire cluster
        if ((cluster_offset == 0) && (nbytes == fat->cluster_byte_size)) {
            int err = dev_write(fat->fs->dev_id, start_sector, buf, fat->sec_per_cluster);
            if (err < 0) {
                return total_write;
            }

            curr_write = fat->cluster_byte_size;
        } else {
            // If it crosses clusters, only write a portion of the first cluster
            if (cluster_offset + curr_write > fat->cluster_byte_size) {
                curr_write = fat->cluster_byte_size - cluster_offset;
            }

            fat->curr_sector = -1;
            int err = dev_read(fat->fs->dev_id, start_sector, fat->fat_buffer, fat->sec_per_cluster);
            if (err < 0) {
                return total_write;
            }
            kernel_memcpy(fat->fat_buffer + cluster_offset, buf, curr_write);        
            
            // Write the entire cluster and then copy from it
            err = dev_write(fat->fs->dev_id, start_sector, fat->fat_buffer, fat->sec_per_cluster);
            if (err < 0) {
                return total_write;
            }
        }

        buf += curr_write;
        nbytes -= curr_write;
        total_write += curr_write;
        file->size += curr_write;

        // Move the file pointer forward
		int err = move_file_pos(file, fat, curr_write, 1);
		if (err < 0) {
            return total_write;
        }
    }

    return total_write;
}

/**
 * @brief Close file
 */
void fatfs_close (file_t * file) {
    if (file->mode == O_RDONLY) {
        return;
    }

    fat_t * fat = (fat_t *)file->fs->data;

    diritem_t * item = read_dir_entry(fat, file->p_index);
    if (item == (diritem_t *)0) {
        return;
    }

    item->DIR_FileSize = file->size;
    item->DIR_FstClusHI = (uint16_t )(file->sblk >> 16);
    item->DIR_FstClusL0 = (uint16_t )(file->sblk & 0xFFFF);
    write_dir_entry(fat, item, file->p_index);
}

/**
 * @brief Adjust the file read/write position
 */
int fatfs_seek (file_t * file, uint32_t offset, int dir) {
     // Only support positioning based on the file's beginning.
    if (dir != 0) {
        return -1;
    }

    fat_t * fat = (fat_t *)file->fs->data;
    cluster_t curr_cluster = file->sblk;
    uint32_t curr_pos = 0;
    uint32_t offset_to_move = offset;

    while (offset_to_move > 0) {
        uint32_t c_off = curr_pos % fat->cluster_byte_size;
        uint32_t curr_move = offset_to_move;

        // If it doesn't exceed one cluster, adjust the position directly, no need to move to the next cluster
        if (c_off + curr_move < fat->cluster_byte_size) {
            curr_pos += curr_move;
            break;
        }

        // If it exceeds one cluster, move only within the current cluster
        curr_move = fat->cluster_byte_size - c_off;
        curr_pos += curr_move;
        offset_to_move -= curr_move;

        // Get next cluster
        curr_cluster = cluster_get_next(fat, curr_cluster);
        if (!cluster_is_valid(curr_cluster)) {
            return -1;
        }
    }

    // Record the final position
    file->pos = curr_pos;
    file->cblk = curr_cluster;
    return 0;
}

int fatfs_stat (file_t * file, struct stat *st) {
    return -1;
}

/**
 * @brief Open the directory. Simply reset the position to 0 for reading
 */
int fatfs_opendir (struct _fs_t * fs,const char * name, DIR * dir) {
    dir->index = 0;
    return 0;
}

/**
 * @brief Read a directory entry
 */
int fatfs_readdir (struct _fs_t * fs,DIR* dir, struct dirent * dirent) {
    fat_t * fat = (fat_t *)fs->data;

    // Simple checks
    while (dir->index < fat->root_ent_cnt) {
        diritem_t * item = read_dir_entry(fat, dir->index);
        if (item == (diritem_t *)0) {
            return -1;
        }

        // The end entry; no need for further scanning, and the index should not advance
        if (item->DIR_Name[0] == DIRITEM_NAME_END) {
            break;
        }

        // Only show normal file and dir
        if (item->DIR_Name[0] != DIRITEM_NAME_FREE) {
            file_type_t type = diritem_get_type(item);
            if ((type == FILE_NORMAL) || (type == FILE_DIR)) {
                dirent->index = dir->index++;
                dirent->type = diritem_get_type(item);
                dirent->size = item->DIR_FileSize;
                diritem_get_name(item, dirent->name);
                return 0;
            }
        }

        dir->index++;
    }

    return -1;
}

/**
 * @brief Close the file scanning and reading
 */
int fatfs_closedir (struct _fs_t * fs,DIR *dir) {
    return 0;
}

/**
 * @brief Delete file
 */
int fatfs_unlink (struct _fs_t * fs, const char * path) {
    fat_t * fat = (fat_t *)fs->data;

    // Traverse the data area of the root directory to find existing matching entries
    for (int i = 0; i < fat->root_ent_cnt; i++) {
        diritem_t * item = read_dir_entry(fat, i);
        if (item == (diritem_t *)0) {
            return -1;
        }

         // The end entry; no need for further scanning, and the index should not advance
        if (item->DIR_Name[0] == DIRITEM_NAME_END) {
            break;
        }

        // Display only regular files and directories; do not display others
        if (item->DIR_Name[0] == DIRITEM_NAME_FREE) {
            continue;
        }

        // Locate the directory to be opened
        if (diritem_name_match(item, path)) {
            // Release the cluster
            int cluster = (item->DIR_FstClusHI << 16) | item->DIR_FstClusL0;
            cluster_free_chain(fat, cluster);

            // Write the 'diritem' entry
            diritem_t item;
            kernel_memset(&item, 0, sizeof(diritem_t));
            return write_dir_entry(fat, &item, i);
        }
    }

    return -1;
}

fs_op_t fatfs_op = {
    .mount = fatfs_mount,
    .unmount = fatfs_unmount,
    .open = fatfs_open,
    .read = fatfs_read,
    .write = fatfs_write,
    .seek = fatfs_seek,
    .stat = fatfs_stat,
    .close = fatfs_close,

    .opendir = fatfs_opendir,
    .readdir = fatfs_readdir,
    .closedir = fatfs_closedir,
    .unlink = fatfs_unlink,
};