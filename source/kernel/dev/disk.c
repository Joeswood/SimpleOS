/**
 * Disk Drive
 * Disks are numbered starting from sda, sdb, sdc sequentially, and partitions are numbered starting from 0 incrementally
 * Partition 0 corresponds to the entire disk's information.
 */
#include "dev/disk.h"
#include "dev/dev.h"
#include "tools/klib.h"
#include "tools/log.h"
#include "comm/cpu_instr.h"
#include "cpu/irq.h"
#include "core/memory.h"
#include "core/task.h"

static disk_t disk_buf[DISK_CNT];  // Channel structure
static mutex_t mutex;     // Channel mutex
static sem_t op_sem;      // sem of channel ops
static int task_on_op;

/**
 * @brief Send ATA commands, supporting up to 16-bit sectors, which is sufficient for our current program
 */
static void ata_send_cmd (disk_t * disk, uint32_t start_sector, uint32_t sector_count, int cmd) {
    outb(DISK_DRIVE(disk), DISK_DRIVE_BASE | disk->drive);		// Use LBA addressing and set the drive

    // must write the highest byte
	outb(DISK_SECTOR_COUNT(disk), (uint8_t) (sector_count >> 8));	// most highest 8 bits in sector
	outb(DISK_LBA_LO(disk), (uint8_t) (start_sector >> 24));		// 24-31 bits in LBA parameters
	outb(DISK_LBA_MID(disk), 0);									// no supported more than 32 bits
	outb(DISK_LBA_HI(disk), 0);										// no supported more than 32 bits
	outb(DISK_SECTOR_COUNT(disk), (uint8_t) (sector_count));		// most lowest 8 bits in sector
	outb(DISK_LBA_LO(disk), (uint8_t) (start_sector >> 0));			// 0-7 bits in LBA parameters
	outb(DISK_LBA_MID(disk), (uint8_t) (start_sector >> 8));		// 8-15 bits in LBA parameters
	outb(DISK_LBA_HI(disk), (uint8_t) (start_sector >> 16));		// 16-23 bits in LBA parameters

    // select corresponding master and slave disk
	outb(DISK_CMD(disk), (uint8_t)cmd);
}

/**
 * @brief Read the ATA data port
 */
static inline void ata_read_data (disk_t * disk, void * buf, int size) {
    uint16_t * c = (uint16_t *)buf;
    for (int i = 0; i < size / 2; i++) {
        *c++ = inw(DISK_DATA(disk));
    }
}

/**
 * @brief Write the ATA data port
 */
static inline void ata_write_data (disk_t * disk, void * buf, int size) {
    uint16_t * c = (uint16_t *)buf;
    for (int i = 0; i < size / 2; i++) {
        outw(DISK_DATA(disk), *c++);
    }
}

/**
 * @brief Wait for data to arrive from the disk
 */
static inline int ata_wait_data (disk_t * disk) {
    uint8_t status;
	do {
        // wait for data or an error
        status = inb(DISK_STATUS(disk));
        if ((status & (DISK_STATUS_BUSY | DISK_STATUS_DRQ | DISK_STATUS_ERR))
                        != DISK_STATUS_BUSY) {
            break;
        }
    }while (1);

    // check error
    return (status & DISK_STATUS_ERR) ? -1 : 0;
}

/**
 * @brief Print disk info
 */
static void print_disk_info (disk_t * disk) {
    log_printf("%s:", disk->name);
    log_printf("  port_base: %x", disk->port_base);
    log_printf("  total_size: %d m", disk->sector_count * disk->sector_size / 1024 /1024);
    log_printf("  drive: %s", disk->drive == DISK_DISK_MASTER ? "Master" : "Slave");

    // show sector info
    log_printf("  Part info:");
    for (int i = 0; i < DISK_PRIMARY_PART_CNT; i++) {
        partinfo_t * part_info = disk->partinfo + i;
        if (part_info->type != FS_INVALID) {
            log_printf("    %s: type: %x, start sector: %d, count %d",
                    part_info->name, part_info->type,
                    part_info->start_sector, part_info->total_sector);
        }
    }
}

/**
 * @brief Retrieve partition information for the specified index.
 * this operation depends on physical partition allocation, so if the device's partition structure changes, the index may also change, resulting in different results
 */
static int detect_part_info(disk_t * disk) {
    mbr_t mbr;

    // read MBR sector
    ata_send_cmd(disk, 0, 1, DISK_CMD_READ);
    int err = ata_wait_data(disk);
    if (err < 0) {
        log_printf("read mbr failed");
        return err;
    }
    ata_read_data(disk, &mbr, sizeof(mbr));

	// iterate descriptions of the four primary partitions, without considering support for extended partitions
	part_item_t * item = mbr.part_item;
    partinfo_t * part_info = disk->partinfo + 1;
	for (int i = 0; i < MBR_PRIMARY_PART_NR; i++, item++, part_info++) {
		part_info->type = item->system_id;

        // no partition, clear part_info
		if (part_info->type == FS_INVALID) {
			part_info->total_sector = 0;
            part_info->start_sector = 0;
            part_info->disk = (disk_t *)0;
        } else {
            // find in sector, copy info
            kernel_sprintf(part_info->name, "%s%d", disk->name, i + 1);
            part_info->start_sector = item->relative_sectors;
            part_info->total_sector = item->total_sectors;
            part_info->disk = disk;
        }
	}
}

/**
 * @brief Check info in disk
 */
static int identify_disk (disk_t * disk) {
    ata_send_cmd(disk, 0, 0, DISK_CMD_IDENTIFY);

    // check the status; if it's 0, the controller does not exist
    int err = inb(DISK_STATUS(disk));
    if (err == 0) {
        log_printf("%s doesn't exist\n", disk->name);
        return -1;
    }

    // wait for data to be ready. At this point, interrupts are not yet enabled, so polling mode can be used temporarily
    err = ata_wait_data(disk);
    if (err < 0) {
        log_printf("disk[%s]: read failed!\n", disk->name);
        return err;
    }

    // read the returned data, especially uint16_t 100 through 103
    // tested on a disk with a total of 102,400 sectors (0x19000)
    uint16_t buf[256];
    ata_read_data(disk, buf, sizeof(buf));
    disk->sector_count = *(uint32_t *)(buf + 100);
    disk->sector_size = SECTOR_SIZE;            // 512 bytes fixed size

    // partition 0 has all info in disk
    partinfo_t * part = disk->partinfo + 0;
    part->disk = disk;
    kernel_sprintf(part->name, "%s%d", disk->name, 0);
    part->start_sector = 0;
    part->total_sector = disk->sector_count;
    part->type = FS_INVALID;

    // detect info in the disk
    detect_part_info(disk);
    return 0;
}

/**
 * @brief Disk initialization and detection
 */
void disk_init (void) {
    log_printf("Checking disk...");

    // clear all disks to avoid data corruption
    kernel_memset(disk_buf, 0, sizeof(disk_buf));

    // sem and mutex
    mutex_init(&mutex);
    sem_init(&op_sem, 0);     

    // detect each hard disk, read hardware existence, and retrieve related information
    for (int i = 0; i < DISK_PER_CHANNEL; i++) {
        disk_t * disk = disk_buf + i;

        // init different seg
        kernel_sprintf(disk->name, "sd%c", i + 'a');
        disk->drive = (i == 0) ? DISK_DISK_MASTER : DISK_DISK_SLAVE;
        disk->port_base = IOBASE_PRIMARY;
        disk->mutex = &mutex;
        disk->op_sem = &op_sem;

        // identify disk
        int err = identify_disk(disk);
        if (err == 0) {
            print_disk_info(disk);
        }
    }
}


/**
 * @brief Open disk device
 */
int disk_open (device_t * dev) {
    int disk_idx = (dev->minor >> 4) - 0xa;
    int part_idx = dev->minor & 0xF;

    if ((disk_idx >= DISK_CNT) || (part_idx >= DISK_PRIMARY_PART_CNT)) {
        log_printf("device minor error: %d", dev->minor);
        return -1;
    }

    disk_t * disk = disk_buf + disk_idx;
    if (disk->sector_size == 0) {
        log_printf("disk not exist. device:sd%x", dev->minor);
        return -1;
    }

    partinfo_t * part_info = disk->partinfo + part_idx;
    if (part_info->total_sector == 0) {
        log_printf("part not exist. device:sd%x", dev->minor);
        return -1;
    }

    // disk exist, build mapping
    dev->data = part_info;
    irq_install(IRQ14_HARDDISK_PRIMARY, exception_handler_ide_primary);
    irq_enable(IRQ14_HARDDISK_PRIMARY);
    return 0;
}

/**
 * @brief Read disk
 */
int disk_read (device_t * dev, int start_sector, char * buf, int count) {
    // get sector info
    partinfo_t * part_info = (partinfo_t *)dev->data;
    if (!part_info) {
        log_printf("Get part info failed! device = %d", dev->minor);
        return -1;
    }

    disk_t * disk = part_info->disk;
    if (disk == (disk_t *)0) {
        log_printf("No disk for device %d", dev->minor);
        return -1;
    }

    mutex_lock(disk->mutex);
    task_on_op = 1;

    int cnt;
    ata_send_cmd(disk, part_info->start_sector + start_sector, count, DISK_CMD_READ);
    for (cnt = 0; cnt < count; cnt++, buf += disk->sector_size) {
        // use sem to wait for interupt, wait writing to finish
        if (task_current()) {
            sem_wait(disk->op_sem);
        }

        // although there is a call to wait here, it won't actually wait because the operation has already completed
        int err = ata_wait_data(disk);
        if (err < 0) {
            log_printf("disk(%s) read error: start sect %d, count %d", disk->name, start_sector, count);
            break;
        }

        // read data again
        ata_read_data(disk, buf, disk->sector_size);
    }

    mutex_unlock(disk->mutex);
    return cnt;
}

/**
 * @brief Write sector
 */
int disk_write (device_t * dev, int start_sector, char * buf, int count) {
    // get sector info
    partinfo_t * part_info = (partinfo_t *)dev->data;
    if (!part_info) {
        log_printf("Get part info failed! device = %d", dev->minor);
        return -1;
    }

    disk_t * disk = part_info->disk;
    if (disk == (disk_t *)0) {
        log_printf("No disk for device %d", dev->minor);
        return -1;
    }

    mutex_lock(disk->mutex);
    task_on_op = 1;

    int cnt;
    ata_send_cmd(disk, part_info->start_sector + start_sector, count, DISK_CMD_WRITE);
    for (cnt = 0; cnt < count; cnt++, buf += disk->sector_size) {
        // write data first
        ata_write_data(disk, buf, disk->sector_size);

        // use sem to wait for interupt, wait writing to finish
        if (task_current()) {
            sem_wait(disk->op_sem);
        }

        int err = ata_wait_data(disk);
        if (err < 0) {
            log_printf("disk(%s) write error: start sect %d, count %d", disk->name, start_sector, count);
            break;
        }
    }

    mutex_unlock(disk->mutex);
    return cnt;
}

/**
 * @brief Send control command to disk
 *
 */
int disk_control (device_t * dev, int cmd, int arg0, int arg1) {
    return 0;
}

/**
 * @brief Close disk
 *
 */
void disk_close (device_t * dev) {
}

/**
 * @brief Disk primary channel interrupt handling
 */
void do_handler_ide_primary (exception_frame_t *frame)  {
    pic_send_eoi(IRQ14_HARDDISK_PRIMARY);
    if (task_on_op && task_current()) {
        sem_notify(&op_sem);
    }
}

// disk device description table
dev_desc_t dev_disk_desc = {
	.name = "disk",
	.major = DEV_DISK,
	.open = disk_open,
	.read = disk_read,
	.write = disk_write,
	.control = disk_control,
	.close = disk_close,
};
