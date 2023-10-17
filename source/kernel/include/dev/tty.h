/**
 * tty
 */
#ifndef TTY_H
#define TTY_H

#include "ipc/sem.h"

#define TTY_NR						8		// max tty device number
#define TTY_IBUF_SIZE				512		// tty input buffer
#define TTY_OBUF_SIZE				512		// tty output buffer
#define TTY_CMD_ECHO				0x1		// enable echo
#define TTY_CMD_IN_COUNT			0x2		

typedef struct _tty_fifo_t {
	char * buf;
	int size;				// max byte number
	int read, write;		// current writing/reading position
	int count;				// current data counter
}tty_fifo_t;

int tty_fifo_get (tty_fifo_t * fifo, char * c);
int tty_fifo_put (tty_fifo_t * fifo, char c);

#define TTY_INLCR			(1 << 0)		// \n to \r\n
#define TTY_IECHO			(1 << 2)		// if echo

#define TTY_OCRLF			(1 << 0)		// if \n to \r\n

/**
 * tty device
 */
typedef struct _tty_t {
	char obuf[TTY_OBUF_SIZE];
	tty_fifo_t ofifo;				// output list(fifo)
	sem_t osem;
	char ibuf[TTY_IBUF_SIZE];
	tty_fifo_t ififo;				// output list(fifo)
	sem_t isem;

	int iflags;						// input flag
    int oflags;						// output flag
	int console_idx;				// console index number
}tty_t;

void tty_select (int tty);
void tty_in (char ch);

#endif /* TTY_H */
