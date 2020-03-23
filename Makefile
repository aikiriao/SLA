CC 		    = gcc
AR				= ar
CFLAGS 	  = -std=c89 -O3 -Wall -Wextra -Wpedantic -Wformat=2 -Wconversion -Wmissing-prototypes -Wstrict-prototypes -Wold-style-definition
CPPFLAGS	= -DNDEBUG
LDFLAGS		= -Wall -Wextra -Wpedantic -O3
LDLIBS		= -lm
ARFLAGS		= r

SRCDIR	  = ./src
OBJDIR	  = ./build
TARGETDIR = ./build
INCLUDE		= -I./src/include/private/ -I./src/include/public/

TARGET    = $(TARGETDIR)/sla $(TARGETDIR)/libsla.a
LIBSRCS		= SLABitStream.c SLACoder.c SLADecoder.c SLAEncoder.c SLAPredictor.c SLAUtility.c
CUISRCS		= $(LIBSRCS) main.c wav.c command_line_parser.c

LIBSRCS		:= $(addprefix $(SRCDIR)/, $(LIBSRCS))
CUISRCS		:= $(addprefix $(SRCDIR)/, $(CUISRCS))
LIBOBJS		= $(addprefix $(OBJDIR)/, $(notdir $(LIBSRCS:%.c=%.o)))
CUIOBJS		= $(addprefix $(OBJDIR)/, $(notdir $(CUISRCS:%.c=%.o)))

all: $(TARGET) 

rebuild:
	make clean
	make all

clean:
	rm -f $(LIBOBJS) $(CUIOBJS) $(TARGET)

$(TARGETDIR)/sla : $(CUIOBJS)
	$(CC) $(LDFLAGS) -o $@ $^ 

$(TARGETDIR)/libsla.a : $(LIBOBJS)
	$(AR) $(ARFLAGS) $@ $^

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(INCLUDE) -o $@ -c $<
