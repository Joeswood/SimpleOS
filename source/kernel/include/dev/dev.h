/**
 * Device Interface
 * Ref: https://wiki.osdev.org/Printing_To_Screen
 */
#ifndef DEV_H
#define DEV_H

#include "comm/types.h"

#define DEV_NAME_SIZE               32      // device name size

enum {
    DEV_UNKNOWN = 0,            // unknown device
    DEV_TTY,                // tty device
    DEV_DISK,               //disk device
};

struct _dev_desc_t;

/**
 * @brief Device driver interface
 */
typedef struct _device_t {
    struct _dev_desc_t * desc;      // Device descriptor
    int mode;                       // op mode
    int minor;                      // minor device number
    void * data;                    // dev config
    int open_count;                 // number of open count
}device_t;


/**
 * @brief Device desciptor structure
 */
typedef struct _dev_desc_t {
    char name[DEV_NAME_SIZE];           // device name
    int major;                          // major device number

    int (*open) (device_t * dev) ;
    int (*read) (device_t * dev, int addr, char * buf, int size);
    int (*write) (device_t * dev, int addr, char * buf, int size);
    int (*control) (device_t * dev, int cmd, int arg0, int arg1);
    void (*close) (device_t * dev);
}dev_desc_t;

int dev_open (int major, int minor, void * data);
int dev_read (int dev_id, int addr, char * buf, int size);
int dev_write (int dev_id, int addr, char * buf, int size);
int dev_control (int dev_id, int cmd, int arg0, int arg1);
void dev_close (int dev_id);

#endif // DEV_H
