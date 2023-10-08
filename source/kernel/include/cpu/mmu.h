/**
 * MMU
 */
#ifndef MMU_H
#define MMU_H

#include "comm/types.h"
#include "comm/cpu_instr.h"

#define PDE_CNT             1024
#define PTE_CNT             1024
#define PTE_P       (1 << 0)
#define PTE_W           (1 << 1)
#define PDE_P       (1 << 0)
#define PTE_U           (1 << 2)
#define PDE_U           (1 << 2)

#pragma pack(1)
/**
 * @brief Page-Table Entry
 */
typedef union _pde_t {
    uint32_t v;
    struct {
        uint32_t present : 1;                   // 0 (P) Present; must be 1 to map a 4-KByte page
        uint32_t write_disable : 1;             // 1 (R/W) Read/write, if 0, writes may not be allowe
        uint32_t user_mode_acc : 1;             // 2 (U/S) if 0, user-mode accesses are not allowed t
        uint32_t write_through : 1;             // 3 (PWT) Page-level write-through
        uint32_t cache_disable : 1;             // 4 (PCD) Page-level cache disable
        uint32_t accessed : 1;                  // 5 (A) Accessed
        uint32_t : 1;                           // 6 Ignored;
        uint32_t ps : 1;                        // 7 (PS)
        uint32_t : 4;                           // 11:8 Ignored
        uint32_t phy_pt_addr : 20;              // most significant 20 bits of page table padrr
    };
}pde_t;

/**
 * @brief Page-Table Entry
 */
typedef union _pte_t {
    uint32_t v;
    struct {
        uint32_t present : 1;                   // 0 (P) Present; must be 1 to map a 4-KByte page
        uint32_t write_disable : 1;             // 1 (R/W) Read/write, if 0, writes may not be allowe
        uint32_t user_mode_acc : 1;             // 2 (U/S) if 0, user-mode accesses are not allowed t
        uint32_t write_through : 1;             // 3 (PWT) Page-level write-through
        uint32_t cache_disable : 1;             // 4 (PCD) Page-level cache disable
        uint32_t accessed : 1;                  // 5 (A) Accessed;
        uint32_t dirty : 1;                     // 6 (D) Dirty
        uint32_t pat : 1;                       // 7 PAT
        uint32_t global : 1;                    // 8 (G) Global
        uint32_t : 3;                           // Ignored
        uint32_t phy_page_addr : 20;            // most significant 20 bits
    };
}pte_t;

#pragma pack()

/**
 * @brief Reture the index of vaddr in page dic
 */
static inline uint32_t pde_index (uint32_t vaddr) {
    int index = (vaddr >> 22); // most significant 10 bits
    return index;
}

/**
 * @brief Retrieve pde address
 */
static inline uint32_t pde_paddr (pde_t * pde) {
    return pde->phy_pt_addr << 12;
}

/**
 * @brief Reture index of vaddr in page table
 */
static inline int pte_index (uint32_t vaddr) {
    return (vaddr >> 12) & 0x3FF;   // middle 10 bits
}

/**
 * @brief Retrieve paddr in pte
 */
static inline uint32_t pte_paddr (pte_t * pte) {
    return pte->phy_page_addr << 12;
}

/**
 * @brief Retrieve the permission bits from the page table
 */
static inline uint32_t get_pte_perm (pte_t * pte) {
    return (pte->v & 0x1FF);                 
}


/**
 * @brief Reload all the page table
 */
static inline void mmu_set_page_dir (uint32_t paddr) {
    // convert vaddr to paddr
    write_cr3(paddr);
}

#endif // MMU_H
