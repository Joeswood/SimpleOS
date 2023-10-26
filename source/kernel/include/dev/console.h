/**
 * Console Display
 * Only support VGA mode
 */
#ifndef CONSOLE_H
#define CONSOLE_H

#include "comm/types.h"
#include "dev/tty.h"
#include "ipc/mutex.h"

//Ref:https://wiki.osdev.org/Printing_To_Screen
#define CONSOLE_VIDEO_BASE			0xb8000		// VRAM base addr(32KB)
#define CONSOLE_DISP_ADDR           0xb8000
#define CONSOLE_DISP_END			(0xb8000 + 32*1024)	// VRAM end addr
#define CONSOLE_ROW_MAX				25			// max row
#define CONSOLE_COL_MAX				80			// max column

#define ASCII_ESC                   0x1b        // ESC ascii code       

#define	ESC_PARAM_MAX				10			// max number of ESC [ parameters

// color table
typedef enum _cclor_t {
    COLOR_Black			= 0,
    COLOR_Blue			= 1,
    COLOR_Green			= 2,
    COLOR_Cyan			= 3,
    COLOR_Red			= 4,
    COLOR_Magenta		= 5,
    COLOR_Brown			= 6,
    COLOR_Gray			= 7,
    COLOR_Dark_Gray 	= 8,
    COLOR_Light_Blue	= 9,
    COLOR_Light_Green	= 10,
    COLOR_Light_Cyan	= 11,
    COLOR_Light_Red		= 12,
    COLOR_Light_Magenta	= 13,
    COLOR_Yellow		= 14,
    COLOR_White			= 15
}cclor_t;

/**
 * @brief Display char
 */
typedef union {
	struct {
		char c;						// char
		char foreground : 4;		// foreground color
		char background : 3;		// background color
	};

	uint16_t v;
}disp_char_t;

/**
 * Console Display
 */
typedef struct _console_t {
	disp_char_t * disp_base;	// display base addr

    enum {
        CONSOLE_WRITE_NORMAL,			// normal mode
        CONSOLE_WRITE_ESC,				// ESC escape sequence
        CONSOLE_WRITE_SQUARE,         
    }write_state;

    int cursor_row, cursor_col;		// current col and row
    int display_rows, display_cols;	// display col and row
    int old_cursor_col, old_cursor_row;	// saved cursor position
    cclor_t foreground, background;	// foreground/background color

    int esc_param[ESC_PARAM_MAX];	// number of ESC [ ;;
    int curr_param_index;

    mutex_t mutex;                  
}console_t;

int console_init (int idx);
int console_write (tty_t * tty);
void console_close (int dev);
void console_select(int idx);
void console_set_cursor(int idx, int visiable);
#endif /* SRC_UI_TTY_WIDGET_H_ */
