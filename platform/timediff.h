#ifndef TIMEDIFF_H
#define TIMEDIFF_H

#include <sys/time.h>
#include <math.h>

int timeval_subtract (struct timeval * result, struct timeval * x, struct timeval * y); 

static inline suseconds_t timeval_to_usec(const struct timeval * tv)
{
	return tv->tv_sec * 1000000 + tv->tv_usec;
}

static inline void usec_to_timeval(struct timeval * tv, suseconds_t usec)
{
	double s = (double)usec * 0.000001;
	tv->tv_sec = (time_t)floor(s);
	tv->tv_usec = (suseconds_t)((s - tv->tv_sec) * 1000000 + 0.000001);
}

#endif
