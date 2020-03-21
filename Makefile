CC 		    = gcc
CFLAGS 	  = -std=c89 -O3 -g3 -Werror -Wall -Wextra -Wpedantic -Wformat=2 -Wconversion -Wmissing-prototypes -Wstrict-prototypes -Wold-style-definition
CPPFLAGS	= -DNDEBUG
LDFLAGS		= -Wall -Wextra -Wpedantic -O3
LDLIBS		= -lm
SRC				= main.c wav.c command_line_parser.c SLABitStream.c SLACoder.c SLADecoder.c SLAEncoder.c SLAPredictor.c SLAUtility.c 
OBJS	 		= $(SRC:%.c=%.o) 
TARGET    = sla 

all: $(TARGET) 

rebuild:
	make clean
	make all

clean:
	rm -f $(OBJS) $(TARGET)

$(TARGET) : $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) $(LDLIBS) -o $(TARGET)

.c.o:
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $<
