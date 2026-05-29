#ifndef BF_TYPES_H
#define BF_TYPES_H

#include <stdint.h>
#include <stddef.h>

#define BF_TAPE_SIZE        65536
#define BF_STACK_SIZE       4096
#define BF_MAX_PROGRAM      1048576
#define BF_MAX_PROCESSES    64
#define BF_MAX_FILENAME     256
#define BF_MAX_PATH         1024
#define BF_STDIN_BUF        4096

#define BF_STATE_EMPTY      0
#define BF_STATE_RUNNING    1
#define BF_STATE_SLEEPING   2
#define BF_STATE_ZOMBIE     3
#define BF_STATE_STOPPED    4

#define BF_ERR_NONE         0
#define BF_ERR_NOMEM        1
#define BF_ERR_BADPTR       2
#define BF_ERR_STACKOVER    3
#define BF_ERR_STACKUNDER   4
#define BF_ERR_BADSYNTAX    5
#define BF_ERR_EOF          6
#define BF_ERR_TIMEOUT      7
#define BF_ERR_KILLED       8
#define BF_ERR_NOFILE       9

typedef uint8_t  bf_cell_t;
typedef uint32_t bf_ptr_t;
typedef uint32_t bf_ip_t;
typedef int32_t  bf_pid_t;

typedef struct {
    bf_cell_t   tape[BF_TAPE_SIZE];
    bf_ptr_t    dp;
    bf_ip_t     ip;
    char        *program;
    size_t      prog_len;
    bf_ip_t     *loop_table;
    int         state;
    bf_pid_t    pid;
    char        name[BF_MAX_FILENAME];
    int         exit_code;
    int         error;
    long        steps;
    long        step_limit;
    int         input_fd;
    int         output_fd;
    int         error_fd;
    uint8_t     input_buf[BF_STDIN_BUF];
    size_t      input_pos;
    size_t      input_len;
    int         input_eof;
} bf_process_t;

typedef struct {
    bf_process_t    *procs[BF_MAX_PROCESSES];
    int             count;
    bf_pid_t        next_pid;
    bf_pid_t        current;
} bf_scheduler_t;

#endif
