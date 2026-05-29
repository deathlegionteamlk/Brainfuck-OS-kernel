CC      = gcc
CFLAGS  = -O2 -Wall -Wextra -std=c11 -D_POSIX_C_SOURCE=200809L -Iinclude
LDFLAGS =

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    LDFLAGS += -lreadline -lncurses
    CFLAGS  += -DHAVE_READLINE=1
    HAVE_RL := $(shell pkg-config --exists readline 2>/dev/null && echo yes || echo no)
    ifneq ($(HAVE_RL),yes)
        CFLAGS  := $(filter-out -DHAVE_READLINE=1,$(CFLAGS))
        LDFLAGS := $(filter-out -lreadline -lncurses,$(LDFLAGS))
    endif
endif

TARGET  = bfkernel
SRCS    = kernel/bf_main.c \
          kernel/bf_scheduler.c \
          interpreter/bf_interpreter.c \
          fs/bf_fs.c \
          shell/bf_shell.c

OBJS    = $(SRCS:.c=.o)
BFROOT  = ./bfroot

.PHONY: all clean install stdlib run

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

stdlib: $(TARGET)
	mkdir -p $(BFROOT)/bin
	cp stdlib/*.bf $(BFROOT)/bin/
	@echo "stdlib installed to $(BFROOT)/bin/"

install: all stdlib

run: install
	./$(TARGET) -r $(BFROOT)

clean:
	rm -f $(OBJS) $(TARGET)
	rm -rf $(BFROOT)
