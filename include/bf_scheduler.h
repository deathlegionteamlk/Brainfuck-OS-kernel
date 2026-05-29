#ifndef BF_SCHEDULER_H
#define BF_SCHEDULER_H

#include "bf_types.h"

bf_scheduler_t *bf_sched_create(void);
void            bf_sched_destroy(bf_scheduler_t *sched);
bf_pid_t        bf_sched_spawn(bf_scheduler_t *sched, const char *name, const char *prog, size_t len);
int             bf_sched_kill(bf_scheduler_t *sched, bf_pid_t pid);
int             bf_sched_tick(bf_scheduler_t *sched, long steps_per_tick);
bf_process_t   *bf_sched_get(bf_scheduler_t *sched, bf_pid_t pid);
void            bf_sched_list(bf_scheduler_t *sched);
int             bf_sched_wait(bf_scheduler_t *sched, bf_pid_t pid);
int             bf_sched_running_count(bf_scheduler_t *sched);

#endif
