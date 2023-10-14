/**
 * System call interface
 */
#ifndef LIB_SYSCALL_H
#define LIB_SYSCALL_H

#include "core/syscall.h"
#include "os_cfg.h"
#include "fs/file.h"
#include "dev/tty.h"

#include <sys/stat.h>
typedef struct _syscall_args_t {
    int id;
    int arg0;
    int arg1;
    int arg2;
    int arg3;
}syscall_args_t;

int msleep (int ms);
int fork(void);
int getpid(void);
int yield (void);
int execve(const char *name, char * const *argv, char * const *env);
int print_msg(char * fmt, int arg);
int wait(int* status);
void _exit(int status);

int open(const char *name, int flags, ...);
int read(int file, char *ptr, int len);
int write(int file, char *ptr, int len);
int close(int file);
int lseek(int file, int ptr, int dir);
int isatty(int file);
int fstat(int file, struct stat *st);
void * sbrk(ptrdiff_t incr);
int dup (int file);
int ioctl(int fd, int cmd, int arg0, int arg1);

struct dirent {
   int index;         // offset in log
   int type;            // file type
   char name [255];       // file name
   int size;            // file size
};

typedef struct _DIR {
    int index;               // iterate index
    struct dirent dirent;
}DIR;

DIR * opendir(const char * name);
struct dirent* readdir(DIR* dir);
int closedir(DIR *dir);
int unlink(const char *pathname);

#endif //LIB_SYSCALL_H
