/**
 * Keyboard device managment
 */
#include "comm/cpu_instr.h"
#include "cpu/irq.h"
#include "dev/kbd.h"
#include "tools/log.h"
#include "tools/klib.h"
#include "dev/tty.h"

static kbd_state_t kbd_state;	// keyboard status

/**
 * Keyboard mapping table, divided into three categories
 * 'normal' represents the default key values when neither the Shift key nor the Num Lock key is pressed.
 * 'func' represents the key values when either the Shift key or the Num Lock key is pressed
 * 'esc' represents the key values starting with 'esc'
 */
static const key_map_t map_table[256] = {
        [0x2] = {'1', '!'},
        [0x3] = {'2', '@'},
        [0x4] = {'3', '#'},
        [0x5] = {'4', '$'},
        [0x6] = {'5', '%'},
        [0x7] = {'6', '^'},
        [0x08] = {'7', '&'},
        [0x09] = {'8', '*' },
        [0x0A] = {'9', '('},
        [0x0B] = {'0', ')'},
        [0x0C] = {'-', '_'},
        [0x0D] = {'=', '+'},
        [0x0E] = {ASCII_DEL, ASCII_DEL},
        [0x0F] = {'\t', '\t'},
        [0x10] = {'q', 'Q'},
        [0x11] = {'w', 'W'},
        [0x12] = {'e', 'E'},
        [0x13] = {'r', 'R'},
        [0x14] = {'t', 'T'},
        [0x15] = {'y', 'Y'},
        [0x16] = {'u', 'U'},
        [0x17] = {'i', 'I'},
        [0x18] = {'o', 'O'},
        [0x19] = {'p', 'P'},
        [0x1A] = {'[', '{'},
        [0x1B] = {']', '}'},
        [0x1C] = {'\n', '\n'},
        [0x1E] = {'a', 'A'},
        [0x1F] = {'s', 'B'},
        [0x20] = {'d',  'D'},
        [0x21] = {'f', 'F'},
        [0x22] = {'g', 'G'},
        [0x23] = {'h', 'H'},
        [0x24] = {'j', 'J'},
        [0x25] = {'k', 'K'},
        [0x26] = {'l', 'L'},
        [0x27] = {';', ':'},
        [0x28] = {'\'', '"'},
        [0x29] = {'`', '~'},
        [0x2B] = {'\\', '|'},
        [0x2C] = {'z', 'Z'},
        [0x2D] = {'x', 'X'},
        [0x2E] = {'c', 'C'},
        [0x2F] = {'v', 'V'},
        [0x30] = {'b', 'B'},
        [0x31] = {'n', 'N'},
        [0x32] = {'m', 'M'},
        [0x33] = {',', '<'},
        [0x34] = {'.', '>'},
        [0x35] = {'/', '?'},
        [0x39] = {' ', ' '},
};

static inline char get_key(uint8_t key_code) {
    return key_code & 0x7F;
}

static inline int is_make_code(uint8_t key_code) {
    return !(key_code & 0x80);
}

/**
 * Wait for writable data
 */
void kbd_wait_send_ready(void) {
    uint32_t time_out = 100000; 
    while (time_out--) {
        if ((inb(KBD_PORT_STAT) & KBD_STAT_SEND_FULL) == 0) {
            return;
        }
    }
}

/**
 * Write data to keyboard port
 */
void kbd_write(uint8_t port, uint8_t data) {
    kbd_wait_send_ready();
    outb(port, data);
}

/**
 * Wait availabl keyboard data
 */
void kbd_wait_recv_ready(void) {
    uint32_t time_out = 100000;
    while (time_out--) {
        if (inb(KBD_PORT_STAT) & KBD_STAT_RECV_READY) {
            return;
        }
    }
}

/**
 * Read keyboard data
 */
uint8_t kbd_read(void) {
    kbd_wait_recv_ready();
    return inb(KBD_PORT_DATA);
}

/**
 * Update status light in kdb
 */
static void update_led_status (void) {
    int data = 0;

    data = (kbd_state.caps_lock ? 1 : 0) << 0;
    kbd_write(KBD_PORT_DATA, KBD_CMD_RW_LED);
    kbd_write(KBD_PORT_DATA, data);
    kbd_read();
}

static void do_fx_key (int key) {
    int index = key - KEY_F1;
    if (kbd_state.lctrl_press || kbd_state.rctrl_press) {
        tty_select(index);
    }
}

/**
 * Process char normal key
 */
static void do_normal_key (uint8_t raw_code) {
    char key = get_key(raw_code);		// remove most significant
    int is_make = is_make_code(raw_code);

	switch (key) {
	    // shift, alt, ctrl
	case KEY_RSHIFT:
		kbd_state.rshift_press = is_make;  
		break;
	case KEY_LSHIFT:
		kbd_state.lshift_press = is_make;  
		break;
    case KEY_CAPS:  // caps Lock key, set the capitalization state
		if (is_make) {
			kbd_state.caps_lock = ~kbd_state.caps_lock;
			update_led_status();
		}
		break;
    case KEY_ALT:
        kbd_state.lalt_press = is_make;  
        break;
    case KEY_CTRL:
        kbd_state.lctrl_press = is_make;  
        break;
    // function keys: Write to the keyboard buffer, to be handled by the application as needed
    case KEY_F1:
    case KEY_F2:
    case KEY_F3:
    case KEY_F4:
    case KEY_F5:
    case KEY_F6:
    case KEY_F7:
    case KEY_F8:
         do_fx_key(key);
        break;
    case KEY_F9:
    case KEY_F10:
    case KEY_F11:
    case KEY_F12:
    case KEY_SCROLL_LOCK:
    default:
        if (is_make) {
            // retrieve the corresponding character based on the Shift control, with the possibility of case conversion or Shift conversion
            if (kbd_state.rshift_press || kbd_state.lshift_press) {
                key = map_table[key].func;  // 2nd func
            }else {
                key = map_table[key].normal;  // 1nd func
            }

            // perform letter case conversion once again based on Caps Lock
            if (kbd_state.caps_lock) {
                if ((key >= 'A') && (key <= 'Z')) {
                    // uppercase to lowercase
                    key = key - 'A' + 'a';
                } else if ((key >= 'a') && (key <= 'z')) {
                    // lowercase to uppercase
                    key = key - 'a' + 'A';
                }
            }

            tty_in(key);
        }
        break;
    }
}

/**
 * Processing keys starting with E0, only handle function keys, ignore longer sequences
 */
static void do_e0_key (uint8_t raw_code) {
    int key = get_key(raw_code);			
    int is_make = is_make_code(raw_code);	// pressed or released

    // keys starting with E0, mainly for HOME, END, cursor movement, and other function keys
    // set the cursor position and then write directly.
    switch (key) {
        case KEY_CTRL:		// both right Ctrl and left Ctrl have the same value
            kbd_state.rctrl_press = is_make;  // set flag only
            break;
        case KEY_ALT:
            kbd_state.ralt_press = is_make;  // set flag only
            break;
    }
}

/**
 * @brief Key interrupt handling program
 */
void do_handler_kbd(exception_frame_t *frame) {
    static enum {
    	NORMAL,				// normal, no e0 or e1
		BEGIN_E0,			// e0
		BEGIN_E1,			// e1
    }recv_state = NORMAL;

	// check for data; exit if there is none
	uint8_t status = inb(KBD_PORT_STAT);
	if (!(status & KBD_STAT_RECV_READY)) {
        pic_send_eoi(IRQ1_KEYBOARD);
		return;
	}

	// read the key value
    uint8_t raw_code = inb(KBD_PORT_DATA);

	// after reading is completed, you can send an EOI (End of Interrupt) to facilitate the continued response to keyboard interrupts
    pic_send_eoi(IRQ1_KEYBOARD);

	if (raw_code == KEY_E0) {
		// E0 char
		recv_state = BEGIN_E0;
	} else if (raw_code == KEY_E1) {
		// E1 char
		recv_state = BEGIN_E1;
	} else {
		switch (recv_state) {
		case NORMAL:
			do_normal_key(raw_code);
			break;
		case BEGIN_E0:
			do_e0_key(raw_code);
			recv_state = NORMAL;
			break;
		case BEGIN_E1: 
			recv_state = NORMAL;
			break;
		}
	}
}

/**
 * Init keyboard hardware
 */
void kbd_init(void) {
    static int inited = 0;

    if (!inited) {
        update_led_status();

        irq_install(IRQ1_KEYBOARD, (irq_handler_t)exception_handler_kbd);
        irq_enable(IRQ1_KEYBOARD);

        inited = 1;
    }
}
