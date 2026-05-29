#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#define _POSIX_C_SOURCE 200809L
#include "bf_types.h"
#include "bf_interpreter.h"
#include "bf_scheduler.h"
#include "bf_fs.h"
#include "bf_shell.h"

#define BF_FS_ROOT_DEFAULT "./bfroot"

static bf_scheduler_t *g_sched = NULL;

static void cleanup(void) {
    if (g_sched) {
        bf_sched_destroy(g_sched);
        g_sched = NULL;
    }
}

static void usage(const char *name) {
    fprintf(stderr,
        "usage: %s [options] [program.bf]\n"
        "\n"
        "options:\n"
        "  -r <root>    filesystem root (default: ./bfroot)\n"
        "  -e <code>    run inline brainfuck code and exit\n"
        "  -l <limit>   step limit per run (0 = unlimited)\n"
        "  -s           shell mode (default when no program given)\n"
        "  -h           show this help\n"
        "\n"
        "examples:\n"
        "  %s                     launch shell\n"
        "  %s hello.bf            run a .bf file\n"
        "  %s -e '++++++++[>++++++++<-]>.'\n"
        "\n",
        name, name, name, name
    );
}

int main(int argc, char *argv[]) {
    char   root[BF_MAX_PATH]     = BF_FS_ROOT_DEFAULT;
    char   inline_code[BF_MAX_PROGRAM] = {0};
    char   *prog_file            = NULL;
    long   step_limit            = 0;
    int    shell_mode            = 1;
    int    opt;

    while ((opt = getopt(argc, argv, "r:e:l:sh")) != -1) {
        switch (opt) {
            case 'r':
                strncpy(root, optarg, BF_MAX_PATH - 1);
                break;
            case 'e':
                strncpy(inline_code, optarg, BF_MAX_PROGRAM - 1);
                shell_mode = 0;
                break;
            case 'l':
                step_limit = atol(optarg);
                break;
            case 's':
                shell_mode = 1;
                break;
            case 'h':
                usage(argv[0]);
                return 0;
            default:
                usage(argv[0]);
                return 1;
        }
    }

    if (optind < argc) {
        prog_file  = argv[optind];
        shell_mode = 0;
    }

    atexit(cleanup);

    if (bf_fs_init(root) != 0) {
        fprintf(stderr, "bfkernel: failed to init filesystem at '%s'\n", root);
        return 1;
    }

    g_sched = bf_sched_create();
    if (!g_sched) {
        fprintf(stderr, "bfkernel: failed to create scheduler\n");
        return 1;
    }

    if (inline_code[0]) {
        size_t len = strlen(inline_code);
        bf_process_t *proc = bf_proc_create("inline", inline_code, len);
        if (!proc) { fprintf(stderr, "bfkernel: failed to create process\n"); return 1; }
        if (step_limit > 0) proc->step_limit = step_limit;
        int r = bf_run(proc);
        int code = proc->exit_code;
        if (r != BF_ERR_NONE && r != BF_ERR_EOF) {
            fprintf(stderr, "\nbfkernel: %s\n", bf_error_str(r));
            code = 1;
        }
        bf_proc_destroy(proc);
        return code;
    }

    if (prog_file) {
        bf_process_t *proc = bf_proc_create(prog_file, "", 0);
        if (!proc) { fprintf(stderr, "bfkernel: failed to create process\n"); return 1; }

        int r = bf_proc_load_file(proc, prog_file);
        if (r != BF_ERR_NONE) {
            fprintf(stderr, "bfkernel: cannot load '%s': %s\n", prog_file, bf_error_str(r));
            bf_proc_destroy(proc);
            return 1;
        }

        if (step_limit > 0) proc->step_limit = step_limit;
        r = bf_run(proc);
        int code = proc->exit_code;
        if (r != BF_ERR_NONE && r != BF_ERR_EOF) {
            fprintf(stderr, "\nbfkernel: %s (after %ld steps)\n", bf_error_str(r), proc->steps);
            code = 1;
        }
        bf_proc_destroy(proc);
        return code;
    }

    if (shell_mode) {
        bf_shell_t *sh = bf_shell_create(g_sched);
        if (!sh) { fprintf(stderr, "bfkernel: failed to create shell\n"); return 1; }
        sh->step_limit = step_limit;
        bf_shell_run(sh);
        bf_shell_destroy(sh);
    }

    return 0;
}
