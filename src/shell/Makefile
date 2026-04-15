# ============================================================================
# electronOS Shell — Makefile
# ============================================================================

CC      ?= gcc
CFLAGS  ?= -Wall -Wextra -Werror -O2 -D_FORTIFY_SOURCE=2 -fstack-protector-strong -fPIE
LDFLAGS ?= -pie -Wl,-z,relro,-z,now -lreadline -lm

SRCS = main.c commands.c
OBJS = $(SRCS:.c=.o)
TARGET = electronos-shell

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)

# Dependencies
main.o: main.c commands.h
commands.o: commands.c commands.h
