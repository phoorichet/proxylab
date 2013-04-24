CC = gcc
CFLAGS = -g -Wall
LDFLAGS = -lpthread

all: proxy

csapp.o: csapp.c csapp.h
	$(CC) $(CFLAGS) -c csapp.c

proxy.o: proxy.c csapp.h
	$(CC) $(CFLAGS) -c proxy.c

ptypes.o: ptypes.c ptypes.h csapp.o
	$(CC) $(CFLAGS) -c ptypes.c

proxythread.o: proxythread.c proxythread.h
	$(CC) $(CFLAGS) -c proxythread.c

cache.o: cache.c cache.h
	$(CC) $(CFLAGS) -c cache.c

proxy: csapp.o ptypes.o proxythread.o proxy.o cache.o

submit:
	(make clean; cd ..; tar cvf proxylab.tar proxylab-handout)

clean:
	rm -f *~ *.o proxy core

