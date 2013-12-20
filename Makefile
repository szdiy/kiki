#
#Makefile for kiki
#

TARGET = kiki
SRC := kiki.c serial.c mx.c packet.c hacker.c
INC := 
LIBS := -lxml2 

all:
	gcc $(SRC) `pkg-config  --libs --cflags gtk+-2.0  gmodule-2.0` `xml2-config --cflags --libs` -g  $(INC) -o $(TARGET) $(LIBS)

.o:.c


.PHONY:clean
clean:
	@rm -rf *.o $(TARGET)

