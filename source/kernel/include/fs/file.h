/**
 * File Management
 */
#ifndef PFILE_H
#define PFILE_H

#include "comm/types.h"

#define FILE_TABLE_SIZE         2048        // number of openable files
#define FILE_NAME_SIZE          32          // file name size

/**
 * File type
 */
typedef enum _file_type_t {
    FILE_UNKNOWN = 0,
    FILE_TTY = 1,
    FILE_NORMAL,
    FILE_DIR,
} file_type_t;

struct _fs_t;

/**
 * File descriptor
 */
typedef struct _file_t {
    char file_name[FILE_NAME_SIZE];	// File name
    file_type_t type;           // File type
    uint32_t size;              // File size
    int ref;                    // reference counter

    int dev_id;                 // device number to which the file belongs

    int pos;                   	// current position
    int sblk;                   // start block
    int cblk;                   // current block
    int p_index;                // index in parent dir
    int mode;					// write/read mode

    struct _fs_t * fs;          // current file system
} file_t;

file_t * file_alloc (void) ;
void file_free (file_t * file);
void file_table_init (void);
void file_inc_ref (file_t * file);

#endif // PFILE_H
