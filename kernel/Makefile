CC = gcc
AS = as
LD = ld

KERNEL_OBJS = kernel.o start.o
KERNEL_BIN = kernel.img

all: $(KERNEL_BIN)

$(KERNEL_BIN): $(KERNEL_OBJS)
	$(LD) -T kernel.ld -o $(KERNEL_BIN) $(KERNEL_OBJS)

kernel.o: kernel.c kernel.h
	$(CC) -c kernel.c -o kernel.o

start.o: start.S
	$(AS) start.S -o start.o

clean:
	rm -f $(KERNEL_OBJS) $(KERNEL_BIN)