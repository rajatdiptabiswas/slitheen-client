CFLAGS=-g -Wall -ggdb -DDEBUG -DDEBUG_UPSTREAM

TARGETS= socks

all: $(TARGETS)

socks5proxy.o tagging.o ptwist168.o util.o:: socks5proxy.h tagging.h ptwist.h util.h

socks: socks5proxy.o tagging.o ptwist168.o util.o util.h ptwist.h tagging.h socks5proxy.h
	gcc -o $@ $^ -L/usr/local/lib -I/usr/local/include -lpthread -lssl -lcrypto

clean:
	-rm *.o

veryclean: clean
	-rm $(TARGETS)
