CC 		    = gcc
CFLAGS 	  = -std=c89 -Wall -Wextra -Wpedantic -Wformat=2 -Wconversion -O3 -g3
CPPFLAGS	= -DNDEBUG
LDFLAGS		= -Wall -Wextra -Wpedantic -O3
LDLIBS		= -lm
OBJS	 		= main.o wav.o command_line_parser.o SLABitStream.o SLACoder.o SLADecoder.o SLAEncoder.o SLAPredictor.o SLAUtility.o 
TARGET    = sla 

all: $(TARGET) 

rebuild:
	make clean
	make all

clean:
	rm -f $(OBJS) $(TEST_OBJS) $(TARGET)

$(TARGET) : $(OBJS) $(TEST_OBJS)
	$(CC) $(LDFLAGS) $(OBJS) $(LDLIBS) -o $(TARGET)

.c.o:
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $<
