#ifndef BF_SHELL_H
#define BF_SHELL_H

#include "bf_types.h"
#include "bf_scheduler.h"
#include "bf_fs.h"

#define BF_SHELL_PROMPT     "bfsh> "
#define BF_SHELL_MAXCMD     2048
#define BF_SHELL_MAXARGS    64
#define BF_SHELL_HISTORY    128

typedef struct {
    bf_scheduler_t  *sched;
    char            cwd[BF_MAX_PATH];
    char            history[BF_SHELL_HISTORY][BF_SHELL_MAXCMD];
    int             hist_count;
    int             hist_pos;
    int             running;
    long            step_limit;
} bf_shell_t;

bf_shell_t *bf_shell_create(bf_scheduler_t *sched);
void        bf_shell_destroy(bf_shell_t *sh);
void        bf_shell_run(bf_shell_t *sh);
int         bf_shell_exec_line(bf_shell_t *sh, const char *line);

#endif
