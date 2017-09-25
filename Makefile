CFLAGS=-g -Wall -ggdb -DDEBUG -DDEBUG_UPSTREAM

TARGETS= socks

all: $(TARGETS)

socks5proxy.o crypto.o tagging.o ptwist168.o util.o:: socks5proxy.h crypto.h tagging.h ptwist.h util.h

socks: socks5proxy.o crypto.o tagging.o ptwist168.o util.o util.h ptwist.h tagging.h crypto.h socks5proxy.h
	gcc -o $@ $^ -L/usr/local/lib -I/usr/local/include -lpthread -lssl -lcrypto

clean:
	-rm *.o

veryclean: clean
	-rm $(TARGETS)
