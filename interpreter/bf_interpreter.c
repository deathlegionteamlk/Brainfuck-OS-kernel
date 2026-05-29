#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "bf_interpreter.h"

bf_process_t *bf_proc_create(const char *name, const char *program, size_t len) {
    bf_process_t *proc = calloc(1, sizeof(bf_process_t));
    if (!proc) return NULL;

    proc->program = malloc(len + 1);
    if (!proc->program) { free(proc); return NULL; }
    memcpy(proc->program, program, len);
    proc->program[len] = '\0';
    proc->prog_len = len;

    proc->loop_table = calloc(len + 1, sizeof(bf_ip_t));
    if (!proc->loop_table) { free(proc->program); free(proc); return NULL; }

    strncpy(proc->name, name ? name : "unnamed", BF_MAX_FILENAME - 1);
    proc->dp         = 0;
    proc->ip         = 0;
    proc->state      = BF_STATE_STOPPED;
    proc->exit_code  = 0;
    proc->error      = BF_ERR_NONE;
    proc->steps      = 0;
    proc->step_limit = 0;
    proc->input_fd   = STDIN_FILENO;
    proc->output_fd  = STDOUT_FILENO;
    proc->error_fd   = STDERR_FILENO;
    proc->input_pos  = 0;
    proc->input_len  = 0;
    proc->input_eof  = 0;

    if (bf_build_loop_table(proc) != BF_ERR_NONE) {
        bf_proc_destroy(proc);
        return NULL;
    }

    return proc;
}

void bf_proc_destroy(bf_process_t *proc) {
    if (!proc) return;
    free(proc->program);
    free(proc->loop_table);
    free(proc);
}

int bf_proc_load_file(bf_process_t *proc, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return BF_ERR_NOFILE;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (sz <= 0 || sz > BF_MAX_PROGRAM) { fclose(f); return BF_ERR_NOMEM; }

    free(proc->program);
    free(proc->loop_table);

    proc->program = malloc(sz + 1);
    if (!proc->program) { fclose(f); return BF_ERR_NOMEM; }

    fread(proc->program, 1, sz, f);
    proc->program[sz] = '\0';
    proc->prog_len = (size_t)sz;
    fclose(f);

    proc->loop_table = calloc(sz + 1, sizeof(bf_ip_t));
    if (!proc->loop_table) return BF_ERR_NOMEM;

    proc->ip    = 0;
    proc->dp    = 0;
    proc->steps = 0;
    proc->error = BF_ERR_NONE;
    memset(proc->tape, 0, BF_TAPE_SIZE);

    return bf_build_loop_table(proc);
}

int bf_build_loop_table(bf_process_t *proc) {
    bf_ip_t stack[BF_STACK_SIZE];
    int top = 0;

    for (bf_ip_t i = 0; i < (bf_ip_t)proc->prog_len; i++) {
        char c = proc->program[i];
        if (c == '[') {
            if (top >= BF_STACK_SIZE) return BF_ERR_STACKOVER;
            stack[top++] = i;
        } else if (c == ']') {
            if (top == 0) return BF_ERR_STACKUNDER;
            bf_ip_t open = stack[--top];
            proc->loop_table[open] = i;
            proc->loop_table[i]    = open;
        }
    }

    if (top != 0) return BF_ERR_STACKUNDER;
    return BF_ERR_NONE;
}

static int bf_read_cell(bf_process_t *proc) {
    if (proc->input_fd == STDIN_FILENO) {
        int ch = getchar();
        return (ch == EOF) ? -1 : ch;
    }

    if (proc->input_pos >= proc->input_len) {
        if (proc->input_eof) return -1;
        ssize_t n = read(proc->input_fd, proc->input_buf, BF_STDIN_BUF);
        if (n <= 0) { proc->input_eof = 1; return -1; }
        proc->input_len = (size_t)n;
        proc->input_pos = 0;
    }

    return (int)proc->input_buf[proc->input_pos++];
}

static void bf_write_cell(bf_process_t *proc, bf_cell_t val) {
    char c = (char)val;
    if (proc->output_fd == STDOUT_FILENO) {
        putchar(c);
        fflush(stdout);
    } else {
        write(proc->output_fd, &c, 1);
    }
}

int bf_step(bf_process_t *proc) {
    if (proc->ip >= (bf_ip_t)proc->prog_len) {
        proc->state = BF_STATE_ZOMBIE;
        return BF_ERR_EOF;
    }

    if (proc->step_limit > 0 && proc->steps >= proc->step_limit) {
        proc->state    = BF_STATE_ZOMBIE;
        proc->error    = BF_ERR_TIMEOUT;
        return BF_ERR_TIMEOUT;
    }

    char c = proc->program[proc->ip];
    proc->steps++;

    switch (c) {
        case '>':
            proc->dp = (proc->dp + 1) % BF_TAPE_SIZE;
            break;
        case '<':
            proc->dp = (proc->dp == 0) ? BF_TAPE_SIZE - 1 : proc->dp - 1;
            break;
        case '+':
            proc->tape[proc->dp]++;
            break;
        case '-':
            proc->tape[proc->dp]--;
            break;
        case '.':
            bf_write_cell(proc, proc->tape[proc->dp]);
            break;
        case ',': {
            int ch = bf_read_cell(proc);
            proc->tape[proc->dp] = (ch == -1) ? 0 : (bf_cell_t)ch;
            break;
        }
        case '[':
            if (proc->tape[proc->dp] == 0)
                proc->ip = proc->loop_table[proc->ip];
            break;
        case ']':
            if (proc->tape[proc->dp] != 0)
                proc->ip = proc->loop_table[proc->ip];
            break;
        default:
            break;
    }

    proc->ip++;
    if (proc->ip >= (bf_ip_t)proc->prog_len) {
        proc->state = BF_STATE_ZOMBIE;
        return BF_ERR_EOF;
    }

    return BF_ERR_NONE;
}

int bf_run_steps(bf_process_t *proc, long steps) {
    proc->state = BF_STATE_RUNNING;
    for (long i = 0; i < steps; i++) {
        int r = bf_step(proc);
        if (r == BF_ERR_EOF) return BF_ERR_NONE;
        if (r != BF_ERR_NONE) return r;
        if (proc->state != BF_STATE_RUNNING) break;
    }
    return BF_ERR_NONE;
}

int bf_run(bf_process_t *proc) {
    proc->state = BF_STATE_RUNNING;
    while (proc->ip < (bf_ip_t)proc->prog_len && proc->state == BF_STATE_RUNNING) {
        int r = bf_step(proc);
        if (r == BF_ERR_EOF) break;
        if (r != BF_ERR_NONE) return r;
    }
    proc->state    = BF_STATE_ZOMBIE;
    proc->exit_code = 0;
    return BF_ERR_NONE;
}

void bf_proc_reset(bf_process_t *proc) {
    memset(proc->tape, 0, BF_TAPE_SIZE);
    proc->dp        = 0;
    proc->ip        = 0;
    proc->steps     = 0;
    proc->state     = BF_STATE_STOPPED;
    proc->error     = BF_ERR_NONE;
    proc->exit_code = 0;
    proc->input_pos = 0;
    proc->input_len = 0;
    proc->input_eof = 0;
}

void bf_tape_dump(bf_process_t *proc, bf_ptr_t from, bf_ptr_t count) {
    printf("tape[%u..%u]:\n", from, from + count - 1);
    for (bf_ptr_t i = 0; i < count; i++) {
        bf_ptr_t idx = (from + i) % BF_TAPE_SIZE;
        printf("  [%5u] = %3d (0x%02x) '%c'%s\n",
            idx,
            proc->tape[idx],
            proc->tape[idx],
            (proc->tape[idx] >= 32 && proc->tape[idx] < 127) ? proc->tape[idx] : '.',
            (idx == proc->dp) ? " <-- dp" : "");
    }
}

const char *bf_error_str(int err) {
    switch (err) {
        case BF_ERR_NONE:       return "ok";
        case BF_ERR_NOMEM:      return "out of memory";
        case BF_ERR_BADPTR:     return "bad pointer";
        case BF_ERR_STACKOVER:  return "bracket stack overflow";
        case BF_ERR_STACKUNDER: return "unmatched bracket";
        case BF_ERR_BADSYNTAX:  return "syntax error";
        case BF_ERR_EOF:        return "end of program";
        case BF_ERR_TIMEOUT:    return "step limit exceeded";
        case BF_ERR_KILLED:     return "process killed";
        case BF_ERR_NOFILE:     return "file not found";
        default:                return "unknown error";
    }
}
