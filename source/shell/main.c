/**
 * Simple shell interpreter
 */
#include <stdio.h>
#include <string.h>
#include "lib_syscall.h"
#include "main.h"
#include <getopt.h>
#include <stdlib.h>
#include <sys/file.h>
#include "fs/file.h"

static cli_t cli;
static const char * promot = "sh >>";       // command line prompt

/**
 * Display command line prompt
 */
static void show_promot(void) {
    printf("%s", cli.promot);
    fflush(stdout);
}

/**
 * help command
 */
static int do_help(int argc, char **argv) {
    const cli_cmd_t * start = cli.cmd_start;

    // loop to print
    while (start < cli.cmd_end) {
        printf("%s %s\n",  start->name, start->useage);
        start++;
    }
    return 0;
}

/**
 * clear command
 */
static int do_clear (int argc, char ** argv) {
    printf("%s", ESC_CLEAR_SCREEN);
    printf("%s", ESC_MOVE_CURSOR(0, 0));
    return 0;
}

/**
 * echo command
 */
static int do_echo (int argc, char ** argv) {
    // only one argument, input manually
    if (argc == 1) {
        char msg_buf[128];

        fgets(msg_buf, sizeof(msg_buf), stdin);
        msg_buf[sizeof(msg_buf) - 1] = '\0';
        puts(msg_buf);
        return 0;
    }

    int count = 1;    // default - only once
    int ch;
    while ((ch = getopt(argc, argv, "n:h")) != -1) {
        switch (ch) {
            case 'h':
                puts("echo echo any message");
                puts("Usage: echo [-n count] msg");
                optind = 1;        // reset
                return 0;
            case 'n':
                count = atoi(optarg);
                break;
            case '?':
                if (optarg) {
                    fprintf(stderr, "Unknown option: -%s\n", optarg);
                }
                optind = 1;        // reset
                return -1;
        }
    }

    // index beyond last one argument position, no message
    if (optind > argc - 1) {
        fprintf(stderr, "Message is empty \n");
        optind = 1;        // reset
        return -1;
    }

    // loop to print the message
    char * msg = argv[optind];
    for (int i = 0; i < count; i++) {
        puts(msg);
    }
    optind = 1;        // reset
    return 0;
}

/**
 * Quit program
 */
static int do_exit (int argc, char ** argv) {
    exit(0);
    return 0;
}

/**
 * @brief list all the file content 
 */
static int do_less (int argc, char ** argv) {
    int line_mode = 0;

    int ch;
    while ((ch = getopt(argc, argv, "lh")) != -1) {
        switch (ch) {
            case 'h':
                puts("show file content");
                puts("less [-l] file");
                puts("-l show file line by line.");
                break;
            case 'l':
                line_mode = 1;
                break;
            case '?':
                if (optarg) {
                    fprintf(stderr, "Unknown option: -%s\n", optarg);
                }
                optind = 1;        // reset
                return -1;
        }
    }

    // Index beyond position of last argument , no send message
    if (optind > argc - 1) {
        fprintf(stderr, "no file\n");
        optind = 1;        // reset
        return -1;
    }

    FILE * file = fopen(argv[optind], "r");
    if (file == NULL) {
        fprintf(stderr, "open file failed. %s", argv[optind]);
        optind = 1;        // reset
        return -1;
    }

    char * buf = (char *)malloc(255);

    if (line_mode == 0) {
        while (fgets(buf, 255, file) != NULL)  {
            fputs(buf, stdout);
        }
    } else {
        // don't use buffer, read immediately
        setvbuf(stdin, NULL, _IONBF, 0);
        ioctl(0, TTY_CMD_ECHO, 0, 0);
        while (1) {
            char * b = fgets(buf, 255, file);
            if (b == NULL ) {
                break;
            }
            fputs(buf, stdout);

            int ch;
            while ((ch = fgetc(stdin)) != 'n') {
                if (ch == 'q') {
                    goto less_quit;
                }
            }
        }
    less_quit:
    // restore buffer
        setvbuf(stdin, NULL,_IOLBF, BUFSIZ);
        ioctl(0, TTY_CMD_ECHO, 1, 0);
    }
    free(buf);
    fclose(file);
    optind = 1;        // reset
    return 0;
}

/**
 * @brief List log content
 */
static int do_ls (int argc, char ** argv) {
    // open log
	DIR * p_dir = opendir("temp");
	if (p_dir == NULL) {
		printf("open dir failed\n");
		return -1;
	}

    // iterate
	struct dirent * entry;
	while((entry = readdir(p_dir)) != NULL) {
        strlwr(entry->name);
		printf("%c %s %d\n",
                entry->type == FILE_DIR ? 'd' : 'f',
                entry->name,
                entry->size);
	}
	closedir(p_dir);

    return 0;
}

/**
 * @brief Copy file command
 */
static int do_cp (int argc, char ** argv) {
    if (argc < 3) {
        puts("no [from] or no [to]");
        return -1;
    }

    FILE * from, * to;
    from = fopen(argv[1], "rb");
    to = fopen(argv[2], "wb");
    if (!from || !to) {
        puts("open file failed.");
        goto ls_failed;
    }

    char * buf = (char *)malloc(255);
    int size = 0;
    while ((size = fread(buf, 1, 255, from)) > 0) {
        fwrite(buf, 1, size, to);
    }
    free(buf);

ls_failed:
    if (from) {
        fclose(from);
    }
    if (to) {
        fclose(to);
    }
    return 0;
}

/**
 * @brief Remove file command
 */
static int do_remove (int argc, char ** argv) {
    if (argc < 2) {
        fprintf(stderr, "no file");
        return -1;
    }

    int err = unlink(argv[1]);
    if (err < 0) {
        fprintf(stderr, "rm file failed: %s", argv[1]);
        return err;
    }

    return 0;
}

// command list
static const cli_cmd_t cmd_list[] = {
    {
        .name = "help",
		.useage = "help -- list support command",
		.do_func = do_help,
    },
    {
        .name = "clear",
		.useage = "clear -- clear the screen",
		.do_func = do_clear,
    },
	{
		.name = "echo",
		.useage = "echo [-n count] msg  -- echo something",
		.do_func = do_echo,
	},
    {
        .name = "ls",
        .useage = "ls [dir] -- list director",
        .do_func = do_ls,
    },
    {
        .name = "less",
        .useage = "list text file content",
        .do_func = do_less,
    },
    {
        .name = "cp",
        .useage = "cp from to -- copy file",
        .do_func = do_cp,
    },
    {
       .name = "rm",
        .useage = "rm file -- remove file",
        .do_func = do_remove,
    },
    {
        .name = "quit",
        .useage = "quit from shell",
        .do_func = do_exit,
    }
};

/**
 * @brief Init command line
 */
static void cli_init(const char * promot, const cli_cmd_t * cmd_list, int cnt) {
    cli.promot = promot;
    
    memset(cli.curr_input, 0, CLI_INPUT_SIZE);
    
    cli.cmd_start = cmd_list;
    cli.cmd_end = cmd_list + cnt;
}

/**
 * @brief Search in all built-in commands
 */
static const cli_cmd_t * find_builtin (const char * name) {
    for (const cli_cmd_t * cmd = cli.cmd_start; cmd < cli.cmd_end; cmd++) {
        if (strcmp(cmd->name, name) != 0) {
            continue;
        }

        return cmd;
    }

    return (const cli_cmd_t *)0;
}

/**
 * @brief Run built-in commands
 */
static void run_builtin (const cli_cmd_t * cmd, int argc, char ** argv) {
    int ret = cmd->do_func(argc, argv);
    if (ret < 0) {
        fprintf(stderr,ESC_COLOR_ERROR"error: %d\n"ESC_COLOR_DEFAULT, ret);
    }
}

/**
 * @brief Iterate log, check if the file exist, return the path of file
 */
static const char * find_exec_path (const char * file_name) {
    static char path[255];

    int fd = open(file_name, 0);
    if (fd < 0) {
        sprintf(path, "%s.elf", file_name);
        fd = open(path, 0);
        if (fd < 0) {
            return (const char * )0;
        }
        close(fd);
        return path;
    } else {
        close(fd);
        return file_name;
    }
}

/**
 * @brief  Run current file
 */
static void run_exec_file (const char * path, int argc, char ** argv) {
    int pid = fork();
    if (pid < 0) {
        fprintf(stderr, "fork failed: %s", path);
    } else if (pid == 0) {
        // child process
        int err = execve(path, argv, (char * const *)0);
        if (err < 0) {
            fprintf(stderr, "exec failed: %s", path);
        }
        exit(-1);
    } else {
        // wait until child process finish
        int status;
        int pid = wait(&status);
        fprintf(stderr, "cmd %s result: %d, pid = %d\n", path, status, pid);
    }
}

int main (int argc, char **argv) {
	open(argv[0], O_RDWR);
    dup(0);     // std output
    dup(0);     // std error output

   	cli_init(promot, cmd_list, sizeof(cmd_list) / sizeof(cli_cmd_t));
	for (;;) {
        // show prompt and start work
        show_promot();

        // Retrieve input string including enter and '0'
        char * str = fgets(cli.curr_input, CLI_INPUT_SIZE, stdin);
        if (str == (char *)0) {
            // error
            break;
        }


        // remove the enter
        char * cr = strchr(cli.curr_input, '\n');
        if (cr) {
            *cr = '\0';
        }
        cr = strchr(cli.curr_input, '\r');
        if (cr) {
            *cr = '\0';
        }

        int argc = 0;
        char * argv[CLI_MAX_ARG_COUNT];
        memset(argv, 0, sizeof(argv));

        // extract command and search in command table
        const char * space = " ";  
        char *token = strtok(cli.curr_input, space);
        while (token) {
            // record arguments
            argv[argc++] = token;

            // retrieve next position
            token = strtok(NULL, space);
        }

        // no any input, then loop 
        if (argc == 0) {
            continue;
        }

        const cli_cmd_t * cmd = find_builtin(argv[0]);
        if (cmd) {
            run_builtin(cmd, argc, argv);
            continue;
        }

        const char * path = find_exec_path(argv[0]);
        if (path) {
            run_exec_file(path, argc, argv);
            continue;
        }

        // not found command, error
        fprintf(stderr, ESC_COLOR_ERROR"Unknown command: %s\n"ESC_COLOR_DEFAULT, cli.curr_input);
    }

    return 0;
}   