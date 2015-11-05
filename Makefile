OBJECTS=base_server.o connection.o thread.o base.o util.o setup.o

CXXFLAGS=-g -Wall -O2

LIB_NAME=libmc_server.a

$(LIB_NAME):$(OBJECTS)
		ar cru  $(LIB_NAME) $(OBJECTS)
		ranlib $(LIB_NAME)

.PHONY:clean
clean:
	rm $(LIB_NAME) $(OBJECTS)

SOURCES=base_server.cpp connection.cpp thread.cpp base.cpp util.cpp setup.cpp

include $(SOURCES:.cpp=.d)

%.d: %.cpp
	set -e; rm -f $@; \
		$(CC) -MM $(CXXFLAGS) $< > $@.$$$$; \
			sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
				rm -f $@.$$$$
