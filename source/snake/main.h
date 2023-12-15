/**
 * Snake game
 */
#ifndef CMD_H
#define CMD_H

#define ESC_CMD2(Pn, cmd)		    "\x1b["#Pn#cmd
#define ESC_CLEAR_SCREEN		    ESC_CMD2(2, J)	//clear screen


// key config
#define PLAYER1_KEY_UP			'w'
#define PLAYER1_KEY_DOWN		's'
#define PLAYER1_KEY_LEFT		'a'
#define PLAYER1_KEY_RIGHT		'd'
#define PLAYER1_KEY_QUITE		'q'

/**
 * snake body node
 */
typedef struct _body_part_t {
	int row;
	int col;
	struct _body_part_t *next;
}body_part_t;

/*
 * snake struct
 */
typedef struct _snake_t {
	body_part_t * head;

	enum {
		SNAKE_BIT_NONE,
		SNAKE_BIT_ITSELF,
		SNAKE_BIT_WALL,
		SNAKE_BIT_FOOD,
	} status;

	int dir;
}snake_t;

#endif
