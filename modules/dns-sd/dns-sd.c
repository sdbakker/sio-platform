#include <pthread.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include <platform/module.h>

#include "mDNSClientAPI.h"
#include "mDNSPosix.h"

MODULE_INIT(dnssd_comm)
SET_MODULE_VERSION(1,0,0)

static char dnstxt[256];
static char name[32] = "SIOS unstable 01";
static char type[32] = "_sios._udp.";
static char domain[32] = "local.";
static char hostname[32] = "";
static int port = 7770;

sios_param_string(name, name, 32);
sios_param_string(type, type, 32);
sios_param_string(domain, domain, 32);
sios_param_string(hostname, hostname, 32);
sios_param(port, int, port);

static int sios_param_set_servicetext(const char * val, struct sios_param * p)
{
	int i;
	char * txt;
	if (strlen(val) + 1 > 256) {
		warn(MODULE_NAME, "servicetext to long");
		return -1;
	}

	txt = (char *)p->arg;
	strncpy(txt, val, 256);
	for (i=0;i<strlen(txt);i++) {
		if (txt[i] == ',') 
			txt[i] = 1;
	}
	return 0;
}
sios_param(dnstxt, servicetext, dnstxt);

static pthread_t main_dnssd_thread;
static pthread_mutex_t halt_lock = PTHREAD_MUTEX_INITIALIZER;
static volatile int halt = 0;

static mDNS mDNSStorage;
static mDNS_PlatformSupport mDNSPlatformStorage;

static const char * default_service_type = "_sios._udp.";
static const char * default_service_domain = "local.";
static ServiceRecordSet service_record_set;

static void * the_main_dnssd_loop(void * arg)
{
	info(MODULE_NAME, "started");
	while (1) {
		struct timeval tv;
		int n, nfds = 0;
		fd_set readfds;
		
		pthread_mutex_lock(&halt_lock);
		if (halt) {
			pthread_mutex_unlock(&halt_lock);
			return NULL;
		}
		pthread_mutex_unlock(&halt_lock);
		
		tv.tv_sec = 5;
		tv.tv_usec = 0;
		mDNSPosixGetFDSet(&mDNSStorage, &nfds, &readfds, &tv);
		n = select(nfds, &readfds, NULL, NULL, &tv);
		if (n < 0) {
			if (errno != EINTR)
				return NULL;
		} else 
			mDNSPosixProcessFDSet(&mDNSStorage, &readfds);
	}
}
static mDNSBool CheckThatServiceTextIsUsable(const char *serviceText, mDNSBool printExplanation,
                                             mDNSu8 *pStringList, mDNSu16 *pStringListLen)
    // Checks that serviceText is a reasonable service text record 
    // and, if it isn't and printExplanation is true, prints 
    // an explanation of why not.  Also parse the text into 
    // the packed PString buffer denoted by pStringList and 
    // return the length of that buffer in *pStringListLen.
    // Note that this routine assumes that the buffer is 
    // sizeof(RDataBody) bytes long.
{
    mDNSBool result;
    size_t   serviceTextLen;
    
    // Note that parsing a C string into a PString list always 
    // expands the data by one character, so the following 
    // compare is ">=", not ">".  Here's the logic:
    //
    // #1 For a string with not ^A's, the PString length is one 
    //    greater than the C string length because we add a length 
    //    byte.
    // #2 For every regular (not ^A) character you add to the C 
    //    string, you add a regular character to the PString list.
    //    This does not affect the equivalence stated in #1.
    // #3 For every ^A you add to the C string, you add a length 
    //    byte to the PString list but you also eliminate the ^A, 
    //    which again does not affect the equivalence stated in #1.
    
    result = mDNStrue;
    serviceTextLen = strlen(serviceText);
    if (result && strlen(serviceText) >= sizeof(RDataBody)) {
        if (printExplanation) {
            warn(MODULE_NAME, "service text record is too long (must be less than %d characters)", 
                    (int) sizeof(RDataBody) );
        }
        result = mDNSfalse;
    }
    
    // Now break the string up into PStrings delimited by ^A.
    // We know the data will fit so we can ignore buffer overrun concerns. 
    // However, we still have to treat runs long than 255 characters as
    // an error.
    
    if (result) {
        int         lastPStringOffset;
        int         i;
        int         thisPStringLen;
        
        // This algorithm is a little tricky.  We start by copying 
        // the string directly into the output buffer, shifted up by 
        // one byte.  We then fill in the first byte with a ^A. 
        // We then walk backwards through the buffer and, for each 
        // ^A that we find, we replace it with the difference between 
        // its offset and the offset of the last ^A that we found
        // (ie lastPStringOffset).
        
        memcpy(&pStringList[1], serviceText, serviceTextLen);
        pStringList[0] = 1;
        lastPStringOffset = serviceTextLen + 1;
        for (i = serviceTextLen; i >= 0; i--) {
            if ( pStringList[i] == 1 ) {
                thisPStringLen = (lastPStringOffset - i - 1);
                assert(thisPStringLen >= 0);
                if (thisPStringLen > 255) {
                    result = mDNSfalse;
                    if (printExplanation) {
                        warn(MODULE_NAME, "each component of the service text record must be 255 characters or less");
                    }
                    break;
                } else {
                    pStringList[i]    = thisPStringLen;
                    lastPStringOffset = i;
                }
            }
        }
        
        *pStringListLen = serviceTextLen + 1;
    }

    return result;
}

static void registration_callback(mDNS * const m,
				  ServiceRecordSet * const thisRegistration,
				  mStatus status)
{
	switch (status) {
		case mStatus_NoError:
			info(MODULE_NAME, "registered" /*, thisRegistration->RR_SRV.resrec.name.c*/);
			break;
		case mStatus_NameConflict:
			warn(MODULE_NAME, "name conflict" /*, thisRegistration->RR_SRV.resrec.name.c*/);
			status = mDNS_RenameAndReregisterService(m, thisRegistration, mDNSNULL);
			break;
		case mStatus_MemFree:
			info(MODULE_NAME, "memory free'd" /*, thisRegistration->RR_SRV.resrec.name.c*/);
			break;
		default:
			warn(MODULE_NAME, "unknown status %d" /*, thisRegistration->RR_SRV.resrec.name.c*/, (int)status);

	}
}

static int sios_dnssd_register_service(const char * stxt, 
				     const char * hostname,
			    	     const char * type, 
			    	     const char * domain, 
			    	     short port) 
{
	mStatus 	status;
	mDNSOpaque16 	mdns_port;
	domainlabel 	mdns_name;
	domainname 	mdns_type;
	domainname 	mdns_domain;
	mDNSu8		service_text[sizeof(RDataBody)];
	mDNSu16		service_text_len;
	
	status = mStatus_NoError;
	
	if (!type) type = default_service_type;
	if (!domain) domain = default_service_domain;
	
	MakeDomainLabelFromLiteralString(&mdns_name, hostname);
	MakeDomainNameFromDNSNameString(&mdns_type, type);
	MakeDomainNameFromDNSNameString(&mdns_domain, domain);
	if (!CheckThatServiceTextIsUsable(stxt, mDNStrue, service_text, &service_text_len)) 
		return -1;
	
	mdns_port.b[0] = (port >> 8) & 0x0FF;
	mdns_port.b[1] = (port >> 0) & 0x0FF;

	status = mDNS_RegisterService(&mDNSStorage, &service_record_set,
				      &mdns_name, &mdns_type, &mdns_domain,
				      NULL, mdns_port,
				      service_text, service_text_len,
				      NULL, 0,
				      mDNSInterface_Any,
				      registration_callback, &service_record_set);

	if (status == mStatus_NoError) {
		info(MODULE_NAME,"registered service: name '%s', type '%s', domain '%s', port %d",
			hostname, type, 
			domain, port);
	} else 
		return -1;

	return 0;
}

static int sios_dnssd_init() 
{
	int retval;
	mStatus status;
	status = mDNS_Init(&mDNSStorage, &mDNSPlatformStorage,
			   mDNS_Init_NoCache, mDNS_Init_ZeroCacheSize,
			   mDNS_Init_AdvertiseLocalAddresses,
			   mDNS_Init_NoInitCallback, mDNS_Init_NoInitCallbackContext);
	if (status != mStatus_NoError)
		return -1;
	
	/* We create a seperate thread here instead of using a running context because
	 * the processing is too much inside of mDNSCore to encapsulate it into a running
	 * context .
	 */
	retval = pthread_create(&main_dnssd_thread, NULL, the_main_dnssd_loop, NULL);
	if (retval)
		return retval;

	return 0;
}

static void sios_dnssd_deregister() 
{
	mDNS_DeregisterService(&mDNSStorage, &service_record_set);
	info(MODULE_NAME, "Deregistered service");
}

static int sios_dnssd_close() 
{
	sios_dnssd_deregister();

	pthread_mutex_lock(&halt_lock);
	halt = 1;
	pthread_mutex_unlock(&halt_lock);
	
	sios_dnssd_deregister();

	return 0;
}

int dnssd_init(void)
{
	int retval;

	retval = sios_object_register(THIS_MODULE, THIS_CLASS);
	if (retval) {
		err(MODULE_NAME, "failed registering comm object");
		return retval;
	}

	retval = sios_dnssd_init();
	if (retval) {
		err(MODULE_NAME, "failed initializing mDNS");
		sios_object_deregister(THIS_MODULE);
		return retval;
	}
	
	retval = sios_dnssd_register_service( dnstxt, name, type, domain, port);

	if (retval) {
		err(MODULE_NAME, "failed registering service");
		sios_object_deregister(THIS_MODULE);
		sios_dnssd_close();
		mDNS_Close(&mDNSStorage);
	}
	
	return 0;
}


void dnssd_exit(void)
{
	sios_object_deregister(THIS_MODULE);
	sios_dnssd_close();
	mDNS_Close(&mDNSStorage);
}

module_init(dnssd_init)
module_exit(dnssd_exit)
