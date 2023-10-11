//
// Timer
// Ref: https://wiki.osdev.org/Programmable_Interval_Timer
//

#include "dev/time.h"
#include "cpu/irq.h"
#include "comm/cpu_instr.h"
#include "os_cfg.h"
#include "core/task.h"

static uint32_t sys_tick;						// number of tick after system start

/**
 * @brief Interrupt handling function
 */
void do_handler_timer (exception_frame_t *frame) {
    sys_tick++;

    //send EOI first, instead of placing it at the end. 
    //placing it at the end would require the task to switch back to continue, after being switched out.
    pic_send_eoi(IRQ0_TIMER);

    task_time_tick();
}

/**
 * @brief Initialize the hardware timer
 */
static void init_pit (void) {
    uint32_t reload_count = PIT_OSC_FREQ / (1000.0 / OS_TICK_MS);

    //outb(PIT_COMMAND_MODE_PORT, PIT_CHANNLE0 | PIT_LOAD_LOHI | PIT_MODE0);
    outb(PIT_COMMAND_MODE_PORT, PIT_CHANNLE0 | PIT_LOAD_LOHI | PIT_MODE3);
    outb(PIT_CHANNEL0_DATA_PORT, reload_count & 0xFF);   // 加载低8位
    outb(PIT_CHANNEL0_DATA_PORT, (reload_count >> 8) & 0xFF); // 再加载高8位

    irq_install(IRQ0_TIMER, (irq_handler_t)exception_handler_timer);
    irq_enable(IRQ0_TIMER);
}

/**
 * @brief Initialize the timer
 */
void time_init (void) {
    sys_tick = 0;

    init_pit();
}


