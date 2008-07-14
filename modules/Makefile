CC = gcc
#CFLAGS_COMMON = -fno-stack-protector -rdynamic -fPIC -O2 -I. -I.. -I../.. -W -ggdb -D_REENTRANT -DSIOS_FIXED_POINT -DDEBUG -DNEW_OSC
CFLAGS_COMMON =  -rdynamic -fPIC -O2 -I. -I.. -I../.. -W -ggdb -D_REENTRANT -DSIOS_FIXED_POINT -DDEBUG -DNEW_OSC
#CFLAGS_COMMON =  -O2 -I. -I.. -I../.. -W -ggdb -D_REENTRANT -DSIOS_FIXED_POINT -DDEBUG -DNEW_OSC

override CFLAGS +=  $(CFLAGS_COMMON) $(EXTRA_CFLAGS)

SIOS_MATRIX_MODULE = sios_matrix.so
SIOS_MATRIX_MODULE_OBJS = matrix/matrix.o

SIOS_ACCMAG_MODULE = sios_accmag.so
SIOS_ACCMAG_MODULE_OBJS = accmag/accmag.o

SIOS_DNSSD_MODULE = dns-sd_comm.so

SIOS_PWM_BEEP_MODULE = sios_pwm_beep.so
SIOS_PWM_BEEP_OBJS = pwm/pwm_beep.o
SIOS_PWM_BUZZ_MODULE = sios_pwm_buzz.so
SIOS_PWM_BUZZ_OBJS = pwm/pwm_buzz.o

SIOS_LIGHT_MODULE = sios_light.so
SIOS_LIGHT_OBJS = light/light.o

all:
modules: 	$(SIOS_MATRIX_MODULE) \
	 	$(SIOS_MATRIX2_MODULE) \
		$(SIOS_ACCMAG_MODULE) \
		$(SIOS_PWM_BEEP_MODULE) \
		$(SIOS_PWM_BUZZ_MODULE) \
		$(SIOS_LIGHT_MODULE) \
		$(SIOS_DNSSD_MODULE) 

$(SIOS_ACCMAG_MODULE): $(SIOS_ACCMAG_MODULE_OBJS)
#	$(LD) -bundle /usr/lib/bundle1.o -flat_namespace -undefined suppress $+ -o $@
	$(LD) -shared -fPIC $+ -o $@

$(SIOS_MATRIX_MODULE): $(SIOS_MATRIX_MODULE_OBJS)
#	$(LD) -bundle /usr/lib/bundle1.o -flat_namespace -undefined suppress $+ -o $@
	$(LD) -shared -fPIC $+ -o $@

$(SIOS_PWM_BEEP_MODULE): $(SIOS_PWM_BEEP_OBJS)
#	$(LD) -bundle /usr/lib/bundle1.o -flat_namespace -undefined suppress $+ -o $@
	$(LD) -shared -fPIC $+ -o $@

$(SIOS_PWM_BUZZ_MODULE): $(SIOS_PWM_BUZZ_OBJS)
#	$(LD) -bundle /usr/lib/bundle1.o -flat_namespace -undefined suppress $+ -o $@
	$(LD) -shared -fPIC $+ -o $@

$(SIOS_LIGHT_MODULE): $(SIOS_LIGHT_OBJS)
#	$(LD) -bundle /usr/lib/bundle1.o -flat_namespace -undefined suppress $+ -o $@
	$(LD) -shared -fPIC $+ -o $@

$(SIOS_DNSSD_MODULE): 
	$(MAKE) -C dns-sd

clean:
	$(MAKE) -C dns-sd clean
	-rm */*.o
	-rm *.so

