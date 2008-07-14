#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <platform/sios.h>
#include <platform/module.h>
#include <platform/util.h>

#include "pwm_defs.h"

#define PWM_BEEP_DEV	"/dev/sios_pwm0"

MODULE_INIT(pwm_beep_obj)
SET_MODULE_VERSION(1,0,0)

static char pwm_device[36] = PWM_BEEP_DEV;
sios_param_string(device, pwm_device, 36);

int max_steps = 20;
sios_param(max_steps, int, max_steps);

int min_delay = 10;	/* ms */
sios_param(min_delay, int, min_delay);

static int dev_pwm_beep_write(struct sios_source_ctx * ctx, enum sios_event_type action);
static struct sios_source_ctx dev_pwm_beep_src = {
	.type = SIOS_POLL_WRITE,
	.priority = SIOS_PRIORITY_DEFAULT,
	.handler = dev_pwm_beep_write,
};

struct beep
{
	u_char data[7];
	short bytes;

	u_char note;
	u_char duty;
	u_short duration;

	int delay;
};

static struct beep beeps[PWM_BUFSIZE];
static u_short	beep_head;
static u_short	beep_tail;

static int pwm_has_beeps()
{
	int data = (beep_head - beep_tail);
	if (data < 0) data += PWM_BUFSIZE;
	return data;
}
	
static int PWMPutBeepTime(u_char note, u_char duty, u_short duration, int delay)
{
	struct beep *beep;
	
	if (pwm_has_beeps() >= (PWM_BUFSIZE - 1))
		return -1;	
	if (note < PWM_BEEP_BASE_NOTE)
		return -1;
	if (note > 0x7F)
		return -1;
	if (duty > PWM_BEEP_MAX_DUTY)
		return -1;
	if (duration > PWM_BEEP_MAX_DURATION)
		return -1;
	if (delay < min_delay)
		delay = min_delay;	

	beep = &beeps[beep_head];
	
	beep->data[0] = (PWM_WORD_TYPE_NOTE << 4);
	beep->data[1] = note;
	beep->data[2] = duty;
	beep->data[3] = ((PWM_WORD_TYPE_DELAY << 4) | ((duration & 0x0F00) >> 8));
	beep->data[4] = (duration & 0x00FF);
	beep->data[5] = (PWM_WORD_TYPE_DUTY << 4);
	beep->data[6] = 0;
	beep->bytes = 7;
	beep->delay = delay;

	INC_N_WRAP_PTR(beep_head);
	
	return 0;
}

static int PWMPutBeep(u_char note, u_char duty, int delay)
{
	struct beep *beep;
	
	if (pwm_has_beeps() >= (PWM_BUFSIZE - 1))
		return -1;	
	if (note < PWM_BEEP_BASE_NOTE)
		return -1;
	if (note > 0x7F)
		return -1;
	if (duty > PWM_BEEP_MAX_DUTY)
		return -1;
	if (delay < min_delay)
		delay = min_delay;	
	
	beep = &beeps[beep_head];
	
	beep->data[0] = (PWM_WORD_TYPE_NOTE << 4);
	beep->data[1] = note;
	beep->data[2] = duty;
	beep->bytes = 3;
	beep->delay = delay;
	
	INC_N_WRAP_PTR(beep_head);
	
	return 0;
}

#define FREQ_MIN_DURATION	10

static int PWMPutSweep(u_short f1, u_short f2, u_char d1, u_char d2, int freq_duration, int duration, u_short sustain) 
{
	unsigned short f;
	signed short fd;
	unsigned char d;
	signed char dd;
	signed short steps, i;
	signed short fstepsize, dstepsize;
	unsigned short fskip, dskip;
	int delay;
	struct beep *beep;

	if (f1 > PWM_MAX_FREQ) f1 = PWM_MAX_FREQ;
	if (f1 <= 0) f1 = 1;
	if (f2 > PWM_MAX_FREQ) f2 = PWM_MAX_FREQ;
	if (f2 <= 0) f2 = 1;
	if (d1 > PWM_BEEP_MAX_DUTY) d1 = PWM_BEEP_MAX_DUTY;
	if (d2 > PWM_BEEP_MAX_DUTY) d2 = PWM_BEEP_MAX_DUTY;
	if (duration <= 0) return -1;
	if (duration < min_delay) duration = min_delay;;
	freq_duration = FREQ_MIN_DURATION;
	
	f = f1; d = d1;
	fd = f2 - f1;
	dd = d2 - d1;
	
	if (freq_duration < FREQ_MIN_DURATION)
		steps = duration / FREQ_MIN_DURATION;
	else 
		steps = duration / freq_duration;

	if (steps == 0)
		steps = 1;
	
	if (!fd) {
		fstepsize = 0;
		fskip = 1;
	} else {
		fstepsize = fd / steps;
		fskip = (steps / ((fd < 0) ? (-1 * fd) : fd)) + 1;
	}
	if (!dd) {
		dstepsize = 0;
		dskip = 1;
	} else {
		dstepsize = dd / steps;
		dskip = (steps / ((dd < 0) ? (-1 * dd) : dd)) + 1;
	}

	fstepsize += (fd < 0) ? -1 : 1;
	dstepsize += (dd < 0) ? -1 : 1;
	
	delay = (duration / steps);

/*	for (i=0;i<=steps;i++,f+=fstepsize,d+=dstepsize) {
		beep = &beeps[beep_head];
		beep->data[0] = ((PWM_WORD_TYPE_FREQ << 4) | ((f & 0x0F00) >> 8));
		beep->data[1] = (f & 0x00FF);
		beep->data[2] = (PWM_WORD_TYPE_DUTY << 4);
		beep->data[3] = d;
		beep->data[4] = ((PWM_WORD_TYPE_DELAY << 4) | ((freq_duration & 0x0F00) >> 8));
		beep->data[5] = (freq_duration & 0x00FF);
		beep->bytes = 6;
		beep->delay = delay;
		INC_N_WRAP_PTR(beep_head);
	}*/

	for (i=0;i<=steps;i++) {
		if (((i % fskip) == 0) && fd)
			f += fstepsize;
		if (((i % dskip) == 0) && dd)
			d += dstepsize;
		beep = &beeps[beep_head];
		beep->data[0] = ((PWM_WORD_TYPE_FREQ << 4) | ((f & 0x0F00) >> 8));
		beep->data[1] = (f & 0x00FF);
		beep->data[2] = (PWM_WORD_TYPE_DUTY << 4);
		beep->data[3] = d;
		beep->data[4] = ((PWM_WORD_TYPE_DELAY << 4) | ((freq_duration & 0x0F00) >> 8));
		beep->data[5] = (freq_duration & 0x00FF);
		beep->bytes = 6;
		beep->delay = delay;
		INC_N_WRAP_PTR(beep_head);
	}
	if (!sustain) {
		beep = &beeps[beep_head];
		beep->data[0] = (PWM_WORD_TYPE_DUTY << 4);
		beep->data[1] = 0;
		beep->bytes = 2;
		beep->delay = min_delay;
		INC_N_WRAP_PTR(beep_head);
	}
}

static int PWMPutSweepBug(u_short f1, u_short f2, u_char d1, u_char d2, int freq_duration, int duration) 
{
	u_short f, fd;
	u_char d, dd;
	short steps, i;
	short fstepsize, dstepsize;
	int delay;
	struct beep *beep;

	if (f1 > PWM_MAX_FREQ) f1 = PWM_MAX_FREQ;
	if (f2 > PWM_MAX_FREQ) f2 = PWM_MAX_FREQ;
	if (d1 > PWM_BEEP_MAX_DUTY) d1 = PWM_BEEP_MAX_DUTY;
	if (d2 > PWM_BEEP_MAX_DUTY) d2 = PWM_BEEP_MAX_DUTY;
	if (duration <= 0) return -1;
	
	f = f1; d = d1;
	fd = f2 - f1;
	dd = d2 - d1;

	if (freq_duration < FREQ_MIN_DURATION)
		steps = duration / FREQ_MIN_DURATION;
	else 
		steps = duration / freq_duration;
	
	if (!fd)
		fstepsize = 0;
	else
		fstepsize = fd / steps;
	if (!dd)
		dstepsize = 0;
	else
		dstepsize = dd / steps;

	fstepsize += (fd < 0) ? -1 : 1;
	dstepsize += (dd < 0) ? -1 : 1;

	delay = (duration / steps);

	for (i=1;i<=steps;i++,f+=fstepsize,d+=dstepsize) {
		beep = &beeps[beep_head];
		beep->data[0] = ((PWM_WORD_TYPE_FREQ << 4) | ((f & 0x0F00) >> 8));
		beep->data[1] = (f & 0x00FF);
		beep->data[2] = (PWM_WORD_TYPE_DUTY << 4);
		beep->data[3] = d;
		beep->data[4] = ((PWM_WORD_TYPE_DELAY << 4) | ((freq_duration & 0x0F00) >> 8));
		beep->data[5] = (freq_duration & 0x00FF);
		beep->bytes = 6;
		beep->delay = delay;
		INC_N_WRAP_PTR(beep_head);
	}
	beep = &beeps[beep_head];
	beep->data[0] = (PWM_WORD_TYPE_DUTY << 4);
	beep->data[1] = 0;
	beep->bytes = 2;
	beep->delay = min_delay;
	INC_N_WRAP_PTR(beep_head);
}


static int pwm_beep_handler(const char *path, const char *types, lo_arg **argv, 
			      int argc, lo_message msg, void *user_data)
{
	switch(argc) {
		case 3:
			PWMPutBeep((u_char)argv[0]->i, (u_char)argv[1]->i, argv[2]->i);
			break;
		case 4:
			PWMPutBeepTime((u_char)argv[0]->i, (u_char)argv[1]->i, (u_short)argv[2]->i, argv[3]->i);
			break;
		default: 
			warn(MODULE_NAME, "beep: wrong amount of arguments");
	}
	sios_add_source_ctx(&dev_pwm_beep_src);
	return 0;
}

static int pwm_sweep_handler(const char *path, const char *types, lo_arg **argv, 
			      int argc, lo_message msg, void *user_data)
{
	dbg("beep sweep");
	switch(argc) {
		case 6:
			PWMPutSweep((u_short)argv[0]->i, (u_short)argv[1]->i, (u_char)argv[2]->i, 
				    (u_char)argv[3]->i, argv[4]->i, argv[5]->i, 0);
			break;
		case 7:
			PWMPutSweep((u_short)argv[0]->i, (u_short)argv[1]->i, (u_char)argv[2]->i, 
				    (u_char)argv[3]->i, argv[4]->i, argv[5]->i, argv[6]->i);
			break;
		default: 
			warn(MODULE_NAME, "sweep: wrong amount of arguments");
	}
	sios_add_source_ctx(&dev_pwm_beep_src);
	return 0;
}

static int pwm_sweep_bug_handler(const char *path, const char *types, lo_arg **argv, 
			      int argc, lo_message msg, void *user_data)
{
	switch(argc) {
		case 6:
			PWMPutSweepBug((u_short)argv[0]->i, (u_short)argv[1]->i, (u_char)argv[2]->i, 
				    (u_char)argv[3]->i, argv[4]->i, argv[5]->i);
			break;
		default: 
			warn(MODULE_NAME, "sweep: wrong amount of arguments");
	}
	sios_add_source_ctx(&dev_pwm_beep_src);
	return 0;
}

static int dev_pwm_beep_write(struct sios_source_ctx * ctx, enum sios_event_type event) 
{
	struct beep *beep;
	size_t bytes = 0;
	int retval;
	u_char out[7];

	if (!(event & SIOS_EVENT_WRITE) || !pwm_has_beeps())
		return 0;

	beep = &beeps[beep_tail];
	retval = write(ctx->fd, beep->data, beep->bytes);
	if (retval < 0) 
		err(MODULE_NAME, "write error '%s': %s", pwm_device, strerror(errno));

	INC_N_WRAP_PTR(beep_tail);

	if (pwm_has_beeps()) 
		ctx->period = beeps[beep_tail].delay * 1000;

	return !pwm_has_beeps();
}

static int open_pwm_dev(const char * dev)
{
	int fd = open(dev, O_WRONLY | O_NONBLOCK);
	if (fd < 0) {
		err(MODULE_NAME, "error opening '%s': %s", dev, strerror(errno));
		return -1;
	}
	return fd;
}

static void close_pwm_dev(int fd)
{
	close(fd);
}

struct sios_method_desc osc_methods[] = {
	METHOD_DESC_INITIALIZER("beep", "", NULL, pwm_beep_handler, "send beep"),
	METHOD_DESC_INITIALIZER("sweep", "", NULL, pwm_sweep_handler, "sweep to frequency"),
	METHOD_DESC_INITIALIZER("sweepbug", "", NULL, pwm_sweep_bug_handler, "bugged version of sweep"),
};

int pwm_beep_init(void)
{
	int retval = 0;
	int fd = 0;

	sios_object_register(THIS_MODULE, THIS_CLASS);

	retval = open_pwm_dev(pwm_device);
	if (retval < 0) {
		sios_object_deregister(THIS_MODULE);
		err(MODULE_NAME, "error opening pwm device: %s",  pwm_device);
		return -1;
	}

	fd = retval;

	retval = sios_osc_add_method_descs(osc_methods, METHOD_DESCRIPTORS(osc_methods));

	if (retval < 0) {
		sios_object_deregister(THIS_MODULE);
		return -1;
	}

	dev_pwm_beep_src.self = THIS_MODULE;
	dev_pwm_beep_src.fd = fd;
	dev_pwm_beep_src.period = min_delay * 1000;
	return 0;
}


void pwm_beep_exit(void)
{
	sios_del_source_ctx(&dev_pwm_beep_src);
	close_pwm_dev(dev_pwm_beep_src.fd);
	sios_object_deregister(THIS_MODULE);
}

module_init(pwm_beep_init)
module_exit(pwm_beep_exit)
