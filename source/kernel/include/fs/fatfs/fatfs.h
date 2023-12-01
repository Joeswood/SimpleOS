/**
 * FAT File System
 */
#ifndef FAT_H
#define FAT_H

#include "ipc/mutex.h"

#pragma pack(1)    

#define FAT_CLUSTER_INVALID 		0xFFF8      	// invalid cluster number
#define FAT_CLUSTER_FREE          	0x00     	    // idle  cluster number

#define DIRITEM_NAME_FREE               0xE5                
#define DIRITEM_NAME_END                0x00                

#define DIRITEM_ATTR_READ_ONLY          0x01                
#define DIRITEM_ATTR_HIDDEN             0x02                
#define DIRITEM_ATTR_SYSTEM             0x04                
#define DIRITEM_ATTR_VOLUME_ID          0x08                
#define DIRITEM_ATTR_DIRECTORY          0x10               
#define DIRITEM_ATTR_ARCHIVE            0x20              
#define DIRITEM_ATTR_LONG_NAME          0x0F               

#define SFN_LEN                    	 	11             

/**
 * FAT dir item
 */
typedef struct _diritem_t {
    uint8_t DIR_Name[11];                   // filename
    uint8_t DIR_Attr;                      // attribute
    uint8_t DIR_NTRes;
    uint8_t DIR_CrtTimeTeenth;             
    uint16_t DIR_CrtTime;         // create time
    uint16_t DIR_CrtDate;         // create data
    uint16_t DIR_LastAccDate;     // last access time
    uint16_t DIR_FstClusHI;                // most significant 16 bits
    uint16_t DIR_WrtTime;         // modify time
    uint16_t DIR_WrtDate;         // modify date
    uint16_t DIR_FstClusL0;                // least significant 16 bits
    uint32_t DIR_FileSize;                 // file bytes size
} diritem_t;

/**
 * Complete DBR type
 */
typedef struct _dbr_t {
    uint8_t BS_jmpBoot[3];                 // jump code
    uint8_t BS_OEMName[8];                 // OEM name 
    uint16_t BPB_BytsPerSec;               // bytes in each sector
    uint8_t BPB_SecPerClus;                // sectors in each cluster
    uint16_t BPB_RsvdSecCnt;               // reserverd sectors
    uint8_t BPB_NumFATs;                   // FAT entries
    uint16_t BPB_RootEntCnt;               // root dir items
    uint16_t BPB_TotSec16;                 // total sectors
    uint8_t BPB_Media;                     // media types
    uint16_t BPB_FATSz16;                  // at item size
    uint16_t BPB_SecPerTrk;                // sector per channel
    uint16_t BPB_NumHeads;                 // head number
    uint32_t BPB_HiddSec;                  // hidden sectors
    uint32_t BPB_TotSec32;                 // total sectors

	uint8_t BS_DrvNum;                     // disk driver config
	uint8_t BS_Reserved1;				   // reserved bytes
	uint8_t BS_BootSig;                    // extended boot signal
	uint32_t BS_VolID;                     // volumn number
	uint8_t BS_VolLab[11];                 // disk volumn
	uint8_t BS_FileSysType[8];             // file type name
} dbr_t;
#pragma pack()

/**
 * Fat structure
 */
typedef struct _fat_t {
    // FAT file system info
    uint32_t tbl_start;                     // FAT table start sector
    uint32_t tbl_cnt;                       // FAT table count
    uint32_t tbl_sectors;                   // sectors per FAT table
    uint32_t bytes_per_sec;                 // sector size
    uint32_t sec_per_cluster;               // sector per cluster
    uint32_t root_ent_cnt;                  // root entry count
    uint32_t root_start;                    // root dir start sector
    uint32_t data_start;                    // data sector start sector
    uint32_t cluster_byte_size;             // byte size per cluster

    // write/read in file system
    uint8_t * fat_buffer;             		// FAT table entry buffer
    int curr_sector;                        // current buffer sector

    struct _fs_t * fs;                      // current file system
    mutex_t mutex;                         
} fat_t;

typedef uint16_t cluster_t;

#endif // FAT_H
