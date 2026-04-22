# ============================================================================
# electronOS Desktop — Makefile
# ============================================================================

CC      ?= gcc
CFLAGS  ?= -Wall -Wextra -Werror -O2 -D_FORTIFY_SOURCE=2 -fstack-protector-strong -fPIE
LDFLAGS ?= -pie -Wl,-z,relro,-z,now

PKG_CFLAGS  := $(shell pkg-config --cflags sdl2 SDL2_image SDL2_ttf 2>/dev/null)
PKG_LDFLAGS := $(shell pkg-config --libs sdl2 SDL2_image SDL2_ttf 2>/dev/null)

CFLAGS  += $(PKG_CFLAGS)
LDFLAGS += $(PKG_LDFLAGS) -lm -lutil

SRCS = main.c desktop.c wallpaper.c glass.c panel.c dock.c window.c \
       app_terminal.c app_settings.c app_editor.c
OBJS = $(SRCS:.c=.o)
TARGET = electronos-desktop

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)

# Dependencies
main.o: main.c desktop.h window.h
desktop.o: desktop.c desktop.h wallpaper.h glass.h panel.h dock.h window.h \
           app_terminal.h app_settings.h app_editor.h
wallpaper.o: wallpaper.c wallpaper.h
glass.o: glass.c glass.h
panel.o: panel.c panel.h glass.h
dock.o: dock.c dock.h glass.h window.h
window.o: window.c window.h glass.h
app_terminal.o: app_terminal.c app_terminal.h window.h
app_settings.o: app_settings.c app_settings.h glass.h window.h
app_editor.o: app_editor.c app_editor.h window.h
