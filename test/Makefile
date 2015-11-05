
CXXFLAGS=-g -O2 -Wall -I..
LDFLAGS=-levent -levent_pthreads

LIB=../libmc_server.a

all:simple_server.o $(LIB)
	g++ -o simple_server simple_server.o $(LIB) $(LDFLAGS)

simple_server.o:simple_server.cpp
	g++ $(CXXFLAGS) -c simple_server.cpp -o simple_server.o

clean:
	rm simple_server *.o
