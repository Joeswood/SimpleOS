/**
 * Two level loader. Initization and kernel loading.
 */
#ifndef LOADER_H
#define LOADER_H

#include "comm/types.h"
#include "comm/boot_info.h"
#include "comm/cpu_instr.h"

// protected mode entry func
void protect_mode_entry (void);

// memory detection information structure
typedef struct SMAP_entry {
    uint32_t BaseL; // base address uint64_t
    uint32_t BaseH;
    uint32_t LengthL; // length uint64_t
    uint32_t LengthH;
    uint32_t Type; // entry Type
    uint32_t ACPI; // extended
}__attribute__((packed)) SMAP_entry_t;

extern boot_info_t boot_info;

#endif // LOADER_H
