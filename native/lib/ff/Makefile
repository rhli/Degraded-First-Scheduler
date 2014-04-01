CC = gcc
CFLAGS = -O3 -Wall
OBJS = ff.o perf_ff.o
#DFLAGS = -DSSE_BOOST
DFLAGS = 

all: perf_ff

%.o: %.c %.h
	$(CC) $(CFLAGS) $(DFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) $(DFLAGS) -c $< -o $@

perf_ff: $(OBJS)
	$(CC) $(CFLAGS) $(DFLAGS) -o $@ $(OBJS)

clean:
	@rm -f *.o
	@rm -f perf_ff
