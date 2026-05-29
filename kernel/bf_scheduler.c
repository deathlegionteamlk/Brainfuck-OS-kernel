#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bf_scheduler.h"
#include "bf_interpreter.h"

bf_scheduler_t *bf_sched_create(void) {
    bf_scheduler_t *s = calloc(1, sizeof(bf_scheduler_t));
    if (!s) return NULL;
    s->next_pid = 1;
    s->current  = -1;
    return s;
}

void bf_sched_destroy(bf_scheduler_t *sched) {
    if (!sched) return;
    for (int i = 0; i < BF_MAX_PROCESSES; i++) {
        if (sched->procs[i]) {
            bf_proc_destroy(sched->procs[i]);
            sched->procs[i] = NULL;
        }
    }
    free(sched);
}

bf_pid_t bf_sched_spawn(bf_scheduler_t *sched, const char *name, const char *prog, size_t len) {
    int slot = -1;
    for (int i = 0; i < BF_MAX_PROCESSES; i++) {
        if (!sched->procs[i]) { slot = i; break; }
    }
    if (slot == -1) return -1;

    bf_process_t *proc = bf_proc_create(name, prog, len);
    if (!proc) return -1;

    proc->pid           = sched->next_pid++;
    proc->state         = BF_STATE_RUNNING;
    sched->procs[slot]  = proc;
    sched->count++;

    return proc->pid;
}

int bf_sched_kill(bf_scheduler_t *sched, bf_pid_t pid) {
    for (int i = 0; i < BF_MAX_PROCESSES; i++) {
        bf_process_t *p = sched->procs[i];
        if (p && p->pid == pid) {
            p->state     = BF_STATE_ZOMBIE;
            p->error     = BF_ERR_KILLED;
            p->exit_code = -1;
            return 0;
        }
    }
    return -1;
}

int bf_sched_tick(bf_scheduler_t *sched, long steps_per_tick) {
    int active = 0;
    for (int i = 0; i < BF_MAX_PROCESSES; i++) {
        bf_process_t *p = sched->procs[i];
        if (!p || p->state != BF_STATE_RUNNING) continue;

        sched->current = p->pid;
        bf_run_steps(p, steps_per_tick);

        if (p->state == BF_STATE_ZOMBIE) {
            sched->count--;
            bf_proc_destroy(p);
            sched->procs[i] = NULL;
        } else {
            active++;
        }
    }
    sched->current = -1;
    return active;
}

bf_process_t *bf_sched_get(bf_scheduler_t *sched, bf_pid_t pid) {
    for (int i = 0; i < BF_MAX_PROCESSES; i++) {
        if (sched->procs[i] && sched->procs[i]->pid == pid)
            return sched->procs[i];
    }
    return NULL;
}

void bf_sched_list(bf_scheduler_t *sched) {
    printf("%-6s %-20s %-10s %-12s\n", "PID", "NAME", "STATE", "STEPS");
    printf("%-6s %-20s %-10s %-12s\n", "---", "----", "-----", "-----");
    for (int i = 0; i < BF_MAX_PROCESSES; i++) {
        bf_process_t *p = sched->procs[i];
        if (!p) continue;
        const char *st = "?";
        switch (p->state) {
            case BF_STATE_RUNNING:  st = "running";  break;
            case BF_STATE_SLEEPING: st = "sleeping"; break;
            case BF_STATE_ZOMBIE:   st = "zombie";   break;
            case BF_STATE_STOPPED:  st = "stopped";  break;
        }
        printf("%-6d %-20s %-10s %-12ld\n", p->pid, p->name, st, p->steps);
    }
}

int bf_sched_wait(bf_scheduler_t *sched, bf_pid_t pid) {
    bf_process_t *p = bf_sched_get(sched, pid);
    if (!p) return -1;
    while (p->state == BF_STATE_RUNNING) {
        bf_run_steps(p, 100000);
    }
    int code = p->exit_code;
    for (int i = 0; i < BF_MAX_PROCESSES; i++) {
        if (sched->procs[i] && sched->procs[i]->pid == pid) {
            bf_proc_destroy(sched->procs[i]);
            sched->procs[i] = NULL;
            sched->count--;
            break;
        }
    }
    return code;
}

int bf_sched_running_count(bf_scheduler_t *sched) {
    int c = 0;
    for (int i = 0; i < BF_MAX_PROCESSES; i++) {
        if (sched->procs[i] && sched->procs[i]->state == BF_STATE_RUNNING)
            c++;
    }
    return c;
}
