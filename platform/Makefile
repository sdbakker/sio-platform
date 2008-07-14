#CC = gcc-3.4
CFLAGS_COMMON =  -O2 -I. -Wall -ggdb -DSIOS_USE_THREADS -DSIOS_FIXED_POINT -DSIOS_OSC_THREADS -D_REENTRANT -DDEBUG -DNEW_OSC -DENABLE_SYSLOG
CFLAGS_SIOS = -rdynamic -I../extlibs/liboscqs/include
#CFLAGS_SIOS = -dynamic -I../extlibs/liboscqs/include

override CFLAGS += $(CFLAGS_COMMON) $(CFLAGS_SIOS)

SIOS_OBJS = 	main.o \
		core.o \
		module.o \
		class.o \
		object.o \
		source.o \
		jhash.o \
		osc.o \
		param.o \
		xmldump.o \
		timediff.o \
		config-parser.o \
		config-lexer.o \
		config.o 

FPSTEST_OBJS = fps_test.o

all: sios

config-parser.c:
	$(YACC) -d -o config-parser.c config-parser.y

#config-lexer.o: config-parser.h config-parser.o

$(SIOS_OBJS): config-parser.c

sios: $(SIOS_OBJS)
	$(CC) $+ -o $@ $(CFLAGS) $(EXTRA_CFLAGS) -llo -lm -ldl -lfl -lpthread

fpstest: $(FPSTEST_OBJS)
	$(CC) $+ -o $@ $(CFLAGS) $(EXTRA_CFLAGS) -lm -ldl -lpthread

clean:
	-rm *.o
	-rm config-parser.[ch]
	-rm sios
	-rm mDNS/*.o

.PHONY: clean