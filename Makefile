CFLAGS=-g -Wall

TARGETS= testget

all: $(TARGETS)

rclient.o ptwist168.o testget.o:: ptwist.h

testget: testget.c rclient.o ptwist168.o ptwist.h
	gcc -g -o $@ $^ -I/home/sltiheen/Downloads/include/openssl libssl.a libcrypto.a -ldl

clean:
	-rm *.o

veryclean: clean
	-rm $(TARGETS)
