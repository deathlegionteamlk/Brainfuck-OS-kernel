#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <signal.h>
#include <sys/stat.h>
#include "bf_shell.h"
#include "bf_interpreter.h"

#ifdef __linux__
#include <readline/readline.h>
#include <readline/history.h>
#define HAVE_READLINE 1
#else
#define HAVE_READLINE 0
#endif

static bf_shell_t *g_shell = NULL;

static void sigint_handler(int sig) {
    (void)sig;
    if (g_shell) {
        printf("\n[bfkernel] SIGINT — type 'exit' to quit\n");
    }
}

bf_shell_t *bf_shell_create(bf_scheduler_t *sched) {
    bf_shell_t *sh = calloc(1, sizeof(bf_shell_t));
    if (!sh) return NULL;
    sh->sched      = sched;
    sh->running    = 1;
    sh->step_limit = 0;
    strncpy(sh->cwd, "/", BF_MAX_PATH - 1);
    return sh;
}

void bf_shell_destroy(bf_shell_t *sh) {
    free(sh);
}

static void trim(char *s) {
    int l = strlen(s);
    while (l > 0 && isspace((unsigned char)s[l - 1])) s[--l] = '\0';
    char *p = s;
    while (*p && isspace((unsigned char)*p)) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
}

static int split_args(char *line, char *argv[], int max) {
    int argc = 0;
    char *p = line;
    while (*p && argc < max - 1) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        if (*p == '"') {
            p++;
            argv[argc++] = p;
            while (*p && *p != '"') p++;
            if (*p) *p++ = '\0';
        } else {
            argv[argc++] = p;
            while (*p && !isspace((unsigned char)*p)) p++;
            if (*p) *p++ = '\0';
        }
    }
    argv[argc] = NULL;
    return argc;
}

static void cmd_help(void) {
    printf(
        "\nbfkernel shell commands:\n"
        "  run <file.bf>         run a brainfuck program\n"
        "  run -i <code>         run inline brainfuck code\n"
        "  bg <file.bf>          run brainfuck program in background\n"
        "  repl                  interactive brainfuck REPL\n"
        "  ps                    list running processes\n"
        "  kill <pid>            kill a process\n"
        "  wait <pid>            wait for process to finish\n"
        "  ls [path]             list directory\n"
        "  cd <path>             change directory\n"
        "  pwd                   print working directory\n"
        "  cat <file>            print file contents\n"
        "  write <file> <data>   write data to file\n"
        "  mkdir <dir>           create directory\n"
        "  tape <file.bf> [n]    dump tape after running (first n cells)\n"
        "  bench <file.bf>       run and show step count + timing\n"
        "  limit [n]             get/set step limit (0 = unlimited)\n"
        "  history               show command history\n"
        "  clear                 clear screen\n"
        "  uname                 show kernel info\n"
        "  exit                  exit bfkernel\n"
        "\n"
    );
}

static void cmd_uname(void) {
    printf("bfkernel 1.0.0 (Brainfuck OS) -- Linux-hosted BF VM\n");
    printf("Tape: %d cells | Max procs: %d | Max program: %d bytes\n",
        BF_TAPE_SIZE, BF_MAX_PROCESSES, BF_MAX_PROGRAM);
}

static void cmd_run(bf_shell_t *sh, int argc, char *argv[]) {
    if (argc < 2) { printf("usage: run <file.bf> | run -i <code>\n"); return; }

    char *prog = NULL;
    size_t prog_len = 0;
    char name[BF_MAX_FILENAME] = "inline";

    if (strcmp(argv[1], "-i") == 0) {
        if (argc < 3) { printf("usage: run -i <brainfuck code>\n"); return; }
        prog     = argv[2];
        prog_len = strlen(prog);
    } else {
        char resolved[BF_MAX_PATH];
        bf_fs_resolve(sh->cwd, argv[1], resolved, sizeof(resolved));

        size_t sz;
        prog = bf_fs_read(resolved, &sz);
        if (!prog) {
            char binpath[BF_MAX_PATH];
            snprintf(binpath, sizeof(binpath), "/bin/%s",
                (argv[1][0] == '/') ? argv[1] + 1 : argv[1]);
            prog = bf_fs_read(binpath, &sz);
        }
        if (!prog) {
            printf("run: %s: no such file\n", argv[1]);
            return;
        }
        prog_len = sz;

        strncpy(name, argv[1], BF_MAX_FILENAME - 1);
    }

    bf_process_t *proc = bf_proc_create(name, prog, prog_len);
    if (strcmp(argv[1], "-i") != 0) free(prog);

    if (!proc) { printf("run: failed to create process\n"); return; }

    if (sh->step_limit > 0) proc->step_limit = sh->step_limit;

    int r = bf_run(proc);
    printf("\n");

    if (r != BF_ERR_NONE && r != BF_ERR_EOF) {
        printf("[bfkernel] error: %s (after %ld steps)\n", bf_error_str(r), proc->steps);
    }

    bf_proc_destroy(proc);
}

static void cmd_bg(bf_shell_t *sh, int argc, char *argv[]) {
    if (argc < 2) { printf("usage: bg <file.bf>\n"); return; }

    char resolved[BF_MAX_PATH];
    bf_fs_resolve(sh->cwd, argv[1], resolved, sizeof(resolved));

    size_t sz;
    char *prog = bf_fs_read(resolved, &sz);
    if (!prog) { printf("bg: %s: no such file\n", argv[1]); return; }

    bf_pid_t pid = bf_sched_spawn(sh->sched, argv[1], prog, sz);
    free(prog);

    if (pid < 0) { printf("bg: failed to spawn process\n"); return; }
    printf("[bg] pid %d started\n", pid);
}

static void cmd_repl(bf_shell_t *sh) {
    printf("BF REPL — enter brainfuck code, empty line to run, 'q' to quit\n");
    printf("Tape is persistent across executions. 'reset' clears tape.\n\n");

    bf_process_t *proc = bf_proc_create("repl", "", 0);
    if (!proc) { printf("repl: failed\n"); return; }
    if (sh->step_limit > 0) proc->step_limit = sh->step_limit;

    char line[BF_SHELL_MAXCMD];
    char code[BF_MAX_PROGRAM];
    code[0] = '\0';

    while (1) {
        printf("bf> ");
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) break;
        trim(line);

        if (strcmp(line, "q") == 0 || strcmp(line, "quit") == 0) break;

        if (strcmp(line, "reset") == 0) {
            bf_proc_reset(proc);
            memset(proc->tape, 0, BF_TAPE_SIZE);
            printf("[repl] tape reset\n");
            continue;
        }

        if (strcmp(line, "dump") == 0) {
            bf_ptr_t start = (proc->dp >= 5) ? proc->dp - 5 : 0;
            bf_tape_dump(proc, start, 16);
            continue;
        }

        if (line[0] == '\0') {
            if (strlen(code) == 0) continue;

            free(proc->program);
            free(proc->loop_table);

            proc->program = strdup(code);
            proc->prog_len = strlen(code);
            proc->loop_table = calloc(proc->prog_len + 1, sizeof(bf_ip_t));
            proc->ip = 0;

            int br = bf_build_loop_table(proc);
            if (br != BF_ERR_NONE) {
                printf("[repl] syntax error: %s\n", bf_error_str(br));
                code[0] = '\0';
                continue;
            }

            proc->state = BF_STATE_RUNNING;
            bf_run(proc);
            printf("\n");

            code[0] = '\0';
            continue;
        }

        size_t cur = strlen(code);
        size_t add = strlen(line);
        if (cur + add + 1 < BF_MAX_PROGRAM) {
            strncat(code, line, BF_MAX_PROGRAM - cur - 1);
        }
    }

    bf_proc_destroy(proc);
    printf("[repl] exited\n");
}

static void cmd_tape(bf_shell_t *sh, int argc, char *argv[]) {
    if (argc < 2) { printf("usage: tape <file.bf> [count]\n"); return; }

    char resolved[BF_MAX_PATH];
    bf_fs_resolve(sh->cwd, argv[1], resolved, sizeof(resolved));

    size_t sz;
    char *prog = bf_fs_read(resolved, &sz);
    if (!prog) { printf("tape: %s: no such file\n", argv[1]); return; }

    bf_process_t *proc = bf_proc_create(argv[1], prog, sz);
    free(prog);
    if (!proc) { printf("tape: failed\n"); return; }

    bf_run(proc);

    bf_ptr_t count = (argc >= 3) ? (bf_ptr_t)atoi(argv[2]) : 32;
    bf_tape_dump(proc, 0, count);
    bf_proc_destroy(proc);
}

static void cmd_bench(bf_shell_t *sh, int argc, char *argv[]) {
    if (argc < 2) { printf("usage: bench <file.bf>\n"); return; }

    char resolved[BF_MAX_PATH];
    bf_fs_resolve(sh->cwd, argv[1], resolved, sizeof(resolved));

    size_t sz;
    char *prog = bf_fs_read(resolved, &sz);
    if (!prog) { printf("bench: %s: no such file\n", argv[1]); return; }

    bf_process_t *proc = bf_proc_create(argv[1], prog, sz);
    free(prog);
    if (!proc) { printf("bench: failed\n"); return; }

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    bf_run(proc);
    clock_gettime(CLOCK_MONOTONIC, &t1);

    double ms = (t1.tv_sec - t0.tv_sec) * 1000.0 +
                (t1.tv_nsec - t0.tv_nsec) / 1e6;

    printf("\n[bench] %s: %ld steps in %.2f ms (%.0f steps/sec)\n",
        argv[1], proc->steps, ms,
        ms > 0 ? proc->steps / (ms / 1000.0) : 0.0);

    bf_proc_destroy(proc);
}

static void cmd_ls(bf_shell_t *sh, int argc, char *argv[]) {
    char resolved[BF_MAX_PATH];
    const char *target = (argc >= 2) ? argv[1] : sh->cwd;
    bf_fs_resolve(sh->cwd, target, resolved, sizeof(resolved));

    bf_dirent_t entries[BF_FS_MAX_ENTRIES];
    int count = 0;

    if (bf_fs_list(resolved, entries, BF_FS_MAX_ENTRIES, &count) != 0) {
        printf("ls: cannot access '%s'\n", resolved);
        return;
    }

    for (int i = 0; i < count; i++) {
        if (entries[i].is_dir)
            printf("  \033[1;34m%-30s\033[0m  <dir>\n", entries[i].name);
        else
            printf("  %-30s  %zu bytes\n", entries[i].name, entries[i].size);
    }

    if (count == 0) printf("  (empty)\n");
}

static void cmd_cat(bf_shell_t *sh, int argc, char *argv[]) {
    if (argc < 2) { printf("usage: cat <file>\n"); return; }

    char resolved[BF_MAX_PATH];
    bf_fs_resolve(sh->cwd, argv[1], resolved, sizeof(resolved));

    size_t sz;
    char *data = bf_fs_read(resolved, &sz);
    if (!data) { printf("cat: %s: no such file\n", argv[1]); return; }

    fwrite(data, 1, sz, stdout);
    printf("\n");
    free(data);
}

static void cmd_write(bf_shell_t *sh, int argc, char *argv[]) {
    if (argc < 3) { printf("usage: write <file> <data>\n"); return; }

    char resolved[BF_MAX_PATH];
    bf_fs_resolve(sh->cwd, argv[1], resolved, sizeof(resolved));

    if (bf_fs_write(resolved, argv[2], strlen(argv[2])) != 0)
        printf("write: failed to write '%s'\n", argv[1]);
    else
        printf("wrote %zu bytes to %s\n", strlen(argv[2]), resolved);
}

static void cmd_mkdir(bf_shell_t *sh, int argc, char *argv[]) {
    if (argc < 2) { printf("usage: mkdir <dir>\n"); return; }

    char resolved[BF_MAX_PATH];
    bf_fs_resolve(sh->cwd, argv[1], resolved, sizeof(resolved));

    if (bf_fs_mkdir(resolved) != 0)
        printf("mkdir: cannot create '%s'\n", argv[1]);
}

static void cmd_cd(bf_shell_t *sh, int argc, char *argv[]) {
    const char *target = (argc >= 2) ? argv[1] : "/";
    char resolved[BF_MAX_PATH];
    bf_fs_resolve(sh->cwd, target, resolved, sizeof(resolved));

    if (!bf_fs_isdir(resolved)) {
        printf("cd: %s: not a directory\n", target);
        return;
    }

    strncpy(sh->cwd, resolved, BF_MAX_PATH - 1);
}

static void cmd_history(bf_shell_t *sh) {
    for (int i = 0; i < sh->hist_count; i++) {
        printf("  %3d  %s\n", i + 1, sh->history[i]);
    }
}

int bf_shell_exec_line(bf_shell_t *sh, const char *line_in) {
    char line[BF_SHELL_MAXCMD];
    strncpy(line, line_in, sizeof(line) - 1);
    trim(line);

    if (line[0] == '\0' || line[0] == '#') return 0;

    if (sh->hist_count < BF_SHELL_HISTORY) {
        strncpy(sh->history[sh->hist_count++], line, BF_SHELL_MAXCMD - 1);
    } else {
        memmove(sh->history[0], sh->history[1],
            sizeof(sh->history[0]) * (BF_SHELL_HISTORY - 1));
        strncpy(sh->history[BF_SHELL_HISTORY - 1], line, BF_SHELL_MAXCMD - 1);
    }

    char *argv[BF_SHELL_MAXARGS];
    int argc = split_args(line, argv, BF_SHELL_MAXARGS);
    if (argc == 0) return 0;

    if      (strcmp(argv[0], "help")    == 0) cmd_help();
    else if (strcmp(argv[0], "uname")   == 0) cmd_uname();
    else if (strcmp(argv[0], "run")     == 0) cmd_run(sh, argc, argv);
    else if (strcmp(argv[0], "bg")      == 0) cmd_bg(sh, argc, argv);
    else if (strcmp(argv[0], "repl")    == 0) cmd_repl(sh);
    else if (strcmp(argv[0], "tape")    == 0) cmd_tape(sh, argc, argv);
    else if (strcmp(argv[0], "bench")   == 0) cmd_bench(sh, argc, argv);
    else if (strcmp(argv[0], "ps")      == 0) bf_sched_list(sh->sched);
    else if (strcmp(argv[0], "kill")    == 0) {
        if (argc < 2) { printf("usage: kill <pid>\n"); }
        else {
            int pid = atoi(argv[1]);
            if (bf_sched_kill(sh->sched, pid) == 0)
                printf("killed pid %d\n", pid);
            else
                printf("kill: no such process %d\n", pid);
        }
    }
    else if (strcmp(argv[0], "wait")    == 0) {
        if (argc < 2) { printf("usage: wait <pid>\n"); }
        else {
            int code = bf_sched_wait(sh->sched, atoi(argv[1]));
            printf("process exited with code %d\n", code);
        }
    }
    else if (strcmp(argv[0], "ls")      == 0) cmd_ls(sh, argc, argv);
    else if (strcmp(argv[0], "cd")      == 0) cmd_cd(sh, argc, argv);
    else if (strcmp(argv[0], "pwd")     == 0) printf("%s\n", sh->cwd);
    else if (strcmp(argv[0], "cat")     == 0) cmd_cat(sh, argc, argv);
    else if (strcmp(argv[0], "write")   == 0) cmd_write(sh, argc, argv);
    else if (strcmp(argv[0], "mkdir")   == 0) cmd_mkdir(sh, argc, argv);
    else if (strcmp(argv[0], "history") == 0) cmd_history(sh);
    else if (strcmp(argv[0], "clear")   == 0) printf("\033[2J\033[H");
    else if (strcmp(argv[0], "limit")   == 0) {
        if (argc >= 2) sh->step_limit = atol(argv[1]);
        printf("step limit: %ld (%s)\n", sh->step_limit,
            sh->step_limit == 0 ? "unlimited" : "active");
    }
    else if (strcmp(argv[0], "exit")    == 0 ||
             strcmp(argv[0], "quit")    == 0) {
        sh->running = 0;
    }
    else {
        char trypath[BF_MAX_PATH * 2];
        snprintf(trypath, sizeof(trypath), "%s/bin/%s.bf", BF_FS_ROOT, argv[0]);
        struct stat st;
        if (stat(trypath, &st) == 0) {
            char *newargv[BF_SHELL_MAXARGS + 2];
            newargv[0] = "run";
            char bname[BF_MAX_FILENAME];
            snprintf(bname, sizeof(bname), "/bin/%s.bf", argv[0]);
            newargv[1] = bname;
            for (int i = 1; i < argc; i++) newargv[i + 1] = argv[i];
            int newargc = argc + 1;
            cmd_run(sh, newargc, newargv);
        } else {
            printf("bfsh: %s: command not found\n", argv[0]);
        }
    }

    return 0;
}

void bf_shell_run(bf_shell_t *sh) {
    g_shell = sh;
    signal(SIGINT, sigint_handler);

    cmd_uname();
    printf("\nType 'help' for commands.\n\n");

#if HAVE_READLINE
    char *input;
    while (sh->running) {
        char prompt[64];
        snprintf(prompt, sizeof(prompt), "bfsh:%s> ", sh->cwd);
        input = readline(prompt);
        if (!input) break;
        if (*input) add_history(input);
        bf_shell_exec_line(sh, input);
        free(input);
    }
#else
    char line[BF_SHELL_MAXCMD];
    while (sh->running) {
        printf("bfsh:%s> ", sh->cwd);
        fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) break;
        line[strcspn(line, "\n")] = '\0';
        bf_shell_exec_line(sh, line);
    }
#endif

    printf("\nbfkernel: shutdown\n");
}
