CC 		    = gcc
CFLAGS 	  = -std=c89 -O0 -g3 -Wall -Wextra -Wpedantic -Wformat=2 -Wstrict-aliasing=2 -Wmissing-prototypes -Wstrict-prototypes -Wold-style-definition
LDFLAGS		=
LDLIBS    = -lm
SRC				= test_main.c test.c \
						test_SLABitStream.c test_SLAUtility.c test_SLACoder.c test_SLAPredictor.c test_SLAEncoder.c test_SLADecoder.c test_SLAByteArray.c test_SLAEncodeDecode.c
SRC				+= test_wav.c test_command_line_parser.c
INCLUDE   = -I../src/include/private -I../src/include/public
OBJS	 		= $(SRC:%.c=%.o) 
TARGET    = test 

all: $(TARGET) 

rebuild:
	make clean
	make all

run: $(TARGET)
	./test

clean:
	rm -f $(OBJS) $(TARGET)

$(TARGET) : $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $(TARGET)

.c.o:
	$(CC) $(CFLAGS) $(INCLUDE) -c $<
