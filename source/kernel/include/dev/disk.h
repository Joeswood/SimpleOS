/**
 * Disk Driver
 */
#ifndef DISK_H
#define DISK_H

#include "comm/types.h"
#include "ipc/mutex.h"
#include "ipc/sem.h"

#define PART_NAME_SIZE              32      // partition name
#define DISK_NAME_SIZE              32      // disk name size
#define DISK_CNT                    2       // number of disk
#define DISK_PRIMARY_PART_CNT       (4+1)       // maximum number of primary partitions is 4 
#define DISK_PER_CHANNEL            2       // number of disk per channel

// https://wiki.osdev.org/ATA_PIO_Mode#IDENTIFY_command
// only support primary bus
#define IOBASE_PRIMARY              0x1F0
#define	DISK_DATA(disk)				(disk->port_base + 0)		// data reg
#define	DISK_ERROR(disk)			(disk->port_base + 1)		// error reg
#define	DISK_SECTOR_COUNT(disk)		(disk->port_base + 2)		// sector reg
#define	DISK_LBA_LO(disk)			(disk->port_base + 3)		// LBA reg
#define	DISK_LBA_MID(disk)			(disk->port_base + 4)		// LBA reg
#define	DISK_LBA_HI(disk)			(disk->port_base + 5)		// LBA reg
#define	DISK_DRIVE(disk)			(disk->port_base + 6)		// drive reg
#define	DISK_STATUS(disk)			(disk->port_base + 7)		// status reg
#define	DISK_CMD(disk)				(disk->port_base + 7)		// cmd reg

// ATA cmd
#define	DISK_CMD_IDENTIFY				0xEC	// IDENTIFY cmd
#define	DISK_CMD_READ					0x24	// read cmd
#define	DISK_CMD_WRITE					0x34	// write cmd

// status regs
#define DISK_STATUS_ERR          (1 << 0)    // error
#define DISK_STATUS_DRQ          (1 << 3)    // ready for input or output
#define DISK_STATUS_DF           (1 << 5)    // driver error
#define DISK_STATUS_BUSY         (1 << 7)    // busy

#define	DISK_DRIVE_BASE		    0xE0		// driver number base: 0xA0 + LBA

#pragma pack(1)

/**
 * MBR partition item table 
 */
typedef struct _part_item_t {
    uint8_t boot_active;               // if the partition is active
	uint8_t start_header;              // start header
	uint16_t start_sector : 6;         // start sector
	uint16_t start_cylinder : 10;	    // start cylinder
	uint8_t system_id;	                // file system type
	uint8_t end_header;                // end header
	uint16_t end_sector : 6;           // end sector
	uint16_t end_cylinder : 10;        // end cylinder
	uint32_t relative_sectors;	        // relative sectors compare with driver
	uint32_t total_sectors;            // total sectors
}part_item_t;

#define MBR_PRIMARY_PART_NR	    4   // 4 partition tables

/**
 * MBR structure
 */
typedef  struct _mbr_t {
	uint8_t code[446];                 // boot code area
    part_item_t part_item[MBR_PRIMARY_PART_NR];
	uint8_t boot_sig[2];               // boot signal
}mbr_t;

#pragma pack()

struct _disk_t;

/**
 * @brief Partition type
 */
typedef struct _partinfo_t {
    char name[PART_NAME_SIZE]; // partition name
    struct _disk_t * disk;      // disk belongs to 

    // https://www.win.tue.nl/~aeb/partitions/partition_types-1.html
    enum {
        FS_INVALID = 0x00,      // invalid file system type
        FS_FAT16_0 = 0x06,      // FAT16 file system type
        FS_FAT16_1 = 0x0E,
    }type;

	int start_sector;           // start sector
	int total_sector;           // total sector
}partinfo_t;

/**
 * @brief Disk structure
 */
typedef struct _disk_t {
    char name[DISK_NAME_SIZE];      // disk name

    enum {
        DISK_DISK_MASTER = (0 << 4),     // master dev
        DISK_DISK_SLAVE = (1 << 4),      // slave dev
    }drive;

    uint16_t port_base;             // port base addr
    int sector_size;                // sector size
    int sector_count;               // total sector count
	partinfo_t partinfo[DISK_PRIMARY_PART_CNT];	// parition table
    mutex_t * mutex;             
    sem_t * op_sem;               
}disk_t;

void disk_init (void);

void exception_handler_ide_primary (void);

#endif // DISK_H
