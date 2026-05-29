#ifndef BF_INTERPRETER_H
#define BF_INTERPRETER_H

#include "bf_types.h"

bf_process_t *bf_proc_create(const char *name, const char *program, size_t len);
void          bf_proc_destroy(bf_process_t *proc);
int           bf_proc_load_file(bf_process_t *proc, const char *path);
int           bf_build_loop_table(bf_process_t *proc);
int           bf_run(bf_process_t *proc);
int           bf_step(bf_process_t *proc);
int           bf_run_steps(bf_process_t *proc, long steps);
void          bf_proc_reset(bf_process_t *proc);
void          bf_tape_dump(bf_process_t *proc, bf_ptr_t from, bf_ptr_t count);
const char   *bf_error_str(int err);

#endif
