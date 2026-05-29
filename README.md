# bfkernel

A Linux-hosted Brainfuck OS kernel. Boots a full shell environment where every program is a `.bf` file. Includes a scheduler, virtual filesystem, REPL, and a stdlib of classic programs.

## Build

```bash
make install
```

Requires: gcc, make. Optional: libreadline-dev (for arrow-key history).

```bash
apt install libreadline-dev   # Debian/Ubuntu
```

## Run

```bash
./bfkernel                    # launch shell
./bfkernel hello.bf           # run a .bf file directly
./bfkernel -e '++++++++++[>++++++++++<-]>--.+.+++++++.'   # inline code
./bfkernel -l 1000000         # set step limit
```

## Shell Commands

| Command | Description |
|---|---|
| `run <file.bf>` | run a brainfuck program |
| `run -i <code>` | run inline brainfuck code |
| `bg <file.bf>` | run in background (scheduler) |
| `repl` | interactive BF REPL with persistent tape |
| `ps` | list all processes |
| `kill <pid>` | kill a process |
| `wait <pid>` | wait for process to finish |
| `ls [path]` | list directory |
| `cd <path>` | change directory |
| `pwd` | print working directory |
| `cat <file>` | print file |
| `write <file> <data>` | write data to file |
| `mkdir <dir>` | make directory |
| `tape <file.bf> [n]` | dump tape after run |
| `bench <file.bf>` | run and show step count + timing |
| `limit [n]` | get/set step limit |
| `history` | command history |
| `uname` | kernel info |
| `exit` | shutdown |

## Stdlib Programs (in /bin/)

| File | Does |
|---|---|
| `hello.bf` | Hello World |
| `cat.bf` | cat stdin to stdout |
| `echo.bf` | echo stdin |
| `fibonacci.bf` | print fibonacci sequence |
| `rot13.bf` | ROT13 cipher |
| `squares.bf` | print squares |
| `reverse.bf` | reverse input |
| `uppercase.bf` | uppercase input |
| `99bottles.bf` | 99 bottles of beer |
| `truth.bf` | print "yes" |
| `twoplus.bf` | 2+5 = G (ASCII 71) |

## Architecture

```
bfkernel/
  kernel/
    bf_main.c        entry point, arg parsing, boot
    bf_scheduler.c   round-robin process scheduler
  interpreter/
    bf_interpreter.c tape VM, loop table, step engine
  fs/
    bf_fs.c          virtual filesystem (backed by ./bfroot/)
  shell/
    bf_shell.c       interactive shell + all builtins
  include/
    bf_types.h       all types and constants
    bf_interpreter.h
    bf_scheduler.h
    bf_fs.h
    bf_shell.h
  stdlib/
    *.bf             standard library programs
  Makefile
```

## REPL Example

```
bfsh:/> repl
bf> ++++++++[>++++[>++>+++>+++>+<<<<-]>+>+>->>+[<]<-]>>.
bf>
H
bf> reset
[repl] tape reset
bf> q
```

## Inline Run Example

```
bfsh:/> run -i "++++++++[>++++[>++>+++>+++>+<<<<-]>+>+>->>+[<]<-]>>.>---.+++++++..+++."
Hello
```

## Background Processes

```
bfsh:/> bg /bin/fibonacci.bf
[bg] pid 1 started
bfsh:/> ps
PID    NAME                 STATE      STEPS
---    ----                 -----      -----
1      /bin/fibonacci.bf    running    0
bfsh:/> wait 1
```
