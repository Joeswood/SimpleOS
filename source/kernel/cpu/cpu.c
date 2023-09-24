/**
 * CPU config
 */
#include "comm/cpu_instr.h"
#include "cpu/cpu.h"
#include "cpu/irq.h"
#include "os_cfg.h"
#include "ipc/mutex.h"
#include "core/syscall.h"

static segment_desc_t gdt_table[GDT_TABLE_SIZE];
static mutex_t mutex;

/**
 * @brief Set segment descriptor
 */
void segment_desc_set(int selector, uint32_t base, uint32_t limit, uint16_t attr) {
    segment_desc_t * desc = gdt_table + (selector >> 3);

	// if the limit is relatively long, change the unit of length to 4KB
	if (limit > 0xfffff) {
		attr |= 0x8000;
		limit /= 0x1000;
	}
	desc->limit15_0 = limit & 0xffff;
	desc->base15_0 = base & 0xffff;
	desc->base23_16 = (base >> 16) & 0xff;
	desc->attr = attr | (((limit >> 16) & 0xf) << 8);
	desc->base31_24 = (base >> 24) & 0xff;
}

/**
 * @brief Set gate descriptor
 */
void gate_desc_set(gate_desc_t * desc, uint16_t selector, uint32_t offset, uint16_t attr) {
	desc->offset15_0 = offset & 0xffff;
	desc->selector = selector;
	desc->attr = attr;
	desc->offset31_16 = (offset >> 16) & 0xffff;
}

void gdt_free_sel (int sel) {
    mutex_lock(&mutex);
    gdt_table[sel / sizeof(segment_desc_t)].attr = 0;
    mutex_unlock(&mutex);
}

/**
 * @brief Allocate GDT
 */
int gdt_alloc_desc (void) {
    int i;

    // skip the first entry
    mutex_lock(&mutex);
    for (i = 1; i < GDT_TABLE_SIZE; i++) {
        segment_desc_t * desc = gdt_table + i;
        if (desc->attr == 0) {
            desc->attr = SEG_P_PRESENT;     // mark as occupied
            break;
        }
    }
    mutex_unlock(&mutex);

    return i >= GDT_TABLE_SIZE ? -1 : i * sizeof(segment_desc_t);;
}

/**
 * @brief Init GDT
 */
void init_gdt(void) {
    // clear all
    for (int i = 0; i < GDT_TABLE_SIZE; i++) {
        segment_desc_set(i << 3, 0, 0, 0);
    }

    // data seg
    segment_desc_set(KERNEL_SELECTOR_DS, 0x00000000, 0xFFFFFFFF,
                     SEG_P_PRESENT | SEG_DPL0 | SEG_S_NORMAL | SEG_TYPE_DATA
                     | SEG_TYPE_RW | SEG_D | SEG_G);

    // non-conforming code segments must be used to allow critical resource access operations to be performed by changing the current task's CPL via call gates
    segment_desc_set(KERNEL_SELECTOR_CS, 0x00000000, 0xFFFFFFFF,
                     SEG_P_PRESENT | SEG_DPL0 | SEG_S_NORMAL | SEG_TYPE_CODE
                     | SEG_TYPE_RW | SEG_D | SEG_G);

    // call gate
    gate_desc_set((gate_desc_t *)(gdt_table + (SELECTOR_SYSCALL >> 3)),
            KERNEL_SELECTOR_CS,
            (uint32_t)exception_handler_syscall,
            GATE_P_PRESENT | GATE_DPL3 | GATE_TYPE_SYSCALL | SYSCALL_PARAM_COUNT);

    // load gdt
    lgdt((uint32_t)gdt_table, sizeof(gdt_table));
}

/**
 * @brief Switch to TSS, meaning jumping to achieve task switching
 */
void switch_to_tss (uint32_t tss_selector) {
    far_jump(tss_selector, 0);
}

/**
 * @brief CPU init
 */
void cpu_init (void) {
    mutex_init(&mutex);

    init_gdt();
}
