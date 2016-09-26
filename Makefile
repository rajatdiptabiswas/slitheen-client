CFLAGS=-g -Wall

TARGETS= socks

all: $(TARGETS)

socks5proxy.o crypto.o:: socks5proxy.h crypto.h

socks: socks5proxy.o crypto.o crypto.h socks5proxy.h
	gcc -o $@ $^ -lpthread -lssl -lcrypto

clean:
	-rm *.o

veryclean: clean
	-rm $(TARGETS)
