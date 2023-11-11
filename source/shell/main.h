/**
 * Command line: built-in command, support external command at same time
 */
#ifndef CMD_H
#define CMD_H

#define CLI_INPUT_SIZE              1024            // input buffer
#define	CLI_MAX_ARG_COUNT		    10			    // max received arguments number

#define ESC_CMD2(Pn, cmd)		    "\x1b["#Pn#cmd
#define	ESC_COLOR_ERROR			    ESC_CMD2(31, m)	// red error message
#define	ESC_COLOR_DEFAULT		    ESC_CMD2(39, m)	// default color
#define ESC_CLEAR_SCREEN		    ESC_CMD2(2, J)	// clear all screen
#define	ESC_MOVE_CURSOR(row, col)  "\x1b["#row";"#col"H"

/**
 * Command list
 */
typedef struct _cli_cmd_t {
    const char * name;          // command name
    const char * useage;        // usage
    int(*do_func)(int argc, char **argv);       // callback func
}cli_cmd_t;

/**
 * Command line manager
 */
typedef struct _cli_t {    
    char curr_input[CLI_INPUT_SIZE]; // current input buffer
    const cli_cmd_t * cmd_start;     // command start
    const cli_cmd_t * cmd_end;       // command end
    const char * promot;        	 // prompt
}cli_t;

#endif
