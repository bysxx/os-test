# Makefile

CC = gcc
CFLAGS = -Wall -pthread

OBJS = main.o library.o os_layer.o buffer_cache.o replacement_policy.o flush_thread.o

all: simulator

simulator: $(OBJS)
	$(CC) $(CFLAGS) -o simulator $(OBJS)

main.o: main.c library.h buffer_cache.h
	$(CC) $(CFLAGS) -c main.c

library.o: library.c library.h os_layer.h
	$(CC) $(CFLAGS) -c library.c

os_layer.o: os_layer.c os_layer.h buffer_cache.h
	$(CC) $(CFLAGS) -c os_layer.c

buffer_cache.o: buffer_cache.c buffer_cache.h
	$(CC) $(CFLAGS) -c buffer_cache.c

replacement_policy.o: replacement_policy.c replacement_policy.h
	$(CC) $(CFLAGS) -c replacement_policy.c

flush_thread.o: flush_thread.c flush_thread.h
	$(CC) $(CFLAGS) -c flush_thread.c

clean:
	rm -f *.o simulator
