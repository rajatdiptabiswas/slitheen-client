CFLAGS=-g -Wall

TARGETS= socks

all: $(TARGETS)

rclient.o ptwist168.o testget.o:: ptwist.h

testget: testget.c rclient.o ptwist168.o ptwist.h
	gcc -g -o $@ $^ -I/home/sltiheen/Downloads/include/openssl libssl.a libcrypto.a -ldl

socks: socks5proxy.c
	gcc -o $@ $^ -lpthread -lssl -lcrypto

clean:
	-rm *.o

veryclean: clean
	-rm $(TARGETS)
