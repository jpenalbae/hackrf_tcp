CC=cc
CFLAGS=-Wall -g -ggdb

LDFLAGS=-lhackrf

OBJS = hackrf_tcp.o


%.o: %.c $(OBJS)
	$(CC) $(CFLAGS) -c -o $@ $< 

hackrf_tcp: $(OBJS)
	$(CC) $(OBJS) $(CFLAGS) $(LDFLAGS) -o hackrf_tcp


clean:
	rm -rf hackrf_tcp *.o

