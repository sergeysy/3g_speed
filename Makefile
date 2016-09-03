# Makefile 3gspeed project

CC=g++

CFLAGS=-O0 -g3 -Wall -c -fmessage-length=0

#LDFLAGS=-lgps

SOURCES=main.cpp SpeedMeasure.cpp Inih/ini.c Inih/INIReader.cpp

OBJECTS=$(SOURCES:.cpp=.o)

EXECUTABLE=3gspeed

all: $(SOURCES) $(EXECUTABLE)
    
$(EXECUTABLE): $(OBJECTS) 
		$(CC)   $(OBJECTS) -o $@ $(LDFLAGS)

.cpp.o:
		$(CC) $(CFLAGS) $< -o $@

clean:
		rm -rf *.o  3gspeed
