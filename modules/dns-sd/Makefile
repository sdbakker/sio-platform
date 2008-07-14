CC = gcc
CFLAGS_COMMON = -fno-stack-protector -rdynamic -fPIC -O3 -I. -I../.. -Wall -ggdb -I. -I./mDNS -DNOT_HAVE_SA_LEN -DMDNS_DEBUGMSGS -D_REENTRANT
#CFLAGS_COMMON =  -rdynamic -fPIC -O3 -I. -I../.. -Wall -ggdb -I. -I./mDNS -DNOT_HAVE_SA_LEN -DMDNS_DEBUGMSGS -D_REENTRANT

override CFLAGS +=  $(CFLAGS_COMMON) $(EXTRA_CFLAGS)

SIOS_DNSSD = ../dns-sd_comm.so
SIOS_DNSSD_OBJS = mDNS/mDNS.o mDNS/mDNSPosix.o mDNS/mDNSUNP.o dns-sd.o

all: $(SIOS_DNSSD)

$(SIOS_DNSSD): $(SIOS_DNSSD_OBJS)
	$(LD) -shared $+ -o $@

clean:
	-rm *.o
	-rm mDNS/*.o
	-rm *.so

