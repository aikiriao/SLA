CC 		    = gcc
CFLAGS 	  = -Wall -Wextra -Wpedantic -Wformat=2 -O0 -g3 -std=c89
LDFLAGS		= -lm
OBJS	 		= test_main.o test.o \
						test_SLABitStream.o test_SLAUtility.o test_SLACoder.o test_SLAPredictor.o test_SLAEncoder.o test_SLADecoder.o test_SLAByteArray.o
OBJS			+= test_wav.o test_command_line_parser.o
TARGET    = test 

all: $(TARGET) 

rebuild:
	make clean
	make all

run: $(TARGET)
	./test

clean:
	rm -f $(OBJS) $(TEST_OBJS) $(TARGET)

$(TARGET) : $(OBJS) $(TEST_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) $(TEST_OBJS) -o $(TARGET)

.c.o:
	$(CC) $(CFLAGS) -c $<
