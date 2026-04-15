# ============================================================================
# electronOS Login Manager — Makefile
# ============================================================================

CC      ?= gcc
CFLAGS  ?= -Wall -Wextra -Werror -O2 -D_FORTIFY_SOURCE=2 -fstack-protector-strong -fPIE
LDFLAGS ?= -pie -Wl,-z,relro,-z,now

PKG_CFLAGS  := $(shell pkg-config --cflags sdl2 SDL2_image SDL2_ttf 2>/dev/null)
PKG_LDFLAGS := $(shell pkg-config --libs sdl2 SDL2_image SDL2_ttf 2>/dev/null)

CFLAGS  += $(PKG_CFLAGS)
LDFLAGS += $(PKG_LDFLAGS) -lpam -lm

SRCS = main.c ui.c auth.c
OBJS = $(SRCS:.c=.o)
TARGET = electronos-login

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)

# Dependencies
main.o: main.c ui.h auth.h
ui.o: ui.c ui.h
auth.o: auth.c auth.h
