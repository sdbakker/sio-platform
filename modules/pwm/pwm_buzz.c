#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <platform/sios.h>
#include <platform/module.h>
#include <platform/util.h>

#ifdef SIOS_FIXED_POINT
#include <platform/fixed.h>
#endif

#include "pwm_defs.h"

#define PWM_BUZZ_DEV	"/dev/sios_pwm1"

#define MIN_DELAY	20
#define MIN_DELAY_MP	0.05

MODULE_INIT(pwm_buzz_obj)
SET_MODULE_VERSION(1,1,1)

static char pwm_device[36] = PWM_BUZZ_DEV;
sios_param_string(device, pwm_device, 36);

int max_steps = 20;
sios_param(max_steps, int, max_steps);

int min_delay = 20;	/* ms */
sios_param(min_delay, int, min_delay);

static int dev_pwm_buzz_write(struct sios_source_ctx * ctx, enum sios_event_type action);
static struct sios_source_ctx dev_pwm_buzz_src = {
	.type = SIOS_POLL_WRITE,
	.priority = SIOS_PRIORITY_DEFAULT,
	.handler = dev_pwm_buzz_write,
};

struct buzz
{
	u_char data[6];
	short bytes;
	
	u_char duty;
	u_short duration;

	int delay;
};

static struct buzz buzzes[PWM_BUFSIZE];
static u_short	buzz_head;
static u_short	buzz_tail;

static int pwm_has_buzzes()
{
	int data = (buzz_head - buzz_tail);
	if (data < 0) data += PWM_BUFSIZE;
	return data;
}

static int PWMPutBuzzTime(u_char duty, u_short duration, int delay)
{
	struct buzz *buzz;
	
	if (pwm_has_buzzes() >= (PWM_BUFSIZE - 1))
		return -1;	
	if (duration > PWM_BUZZ_MAX_DURATION)
		return -1;
	if (delay < min_delay) 
		delay = min_delay;
	
	buzz = &buzzes[buzz_head];
	
	buzz->data[0] = (PWM_WORD_TYPE_DUTY << 4);
	buzz->data[1] = duty;
	buzz->data[2] = ((PWM_WORD_TYPE_DELAY << 4) | ((duration & 0x0F00) >> 8));
	buzz->data[3] = (duration & 0x00FF);
	buzz->data[4] = (PWM_WORD_TYPE_DUTY << 4);
	buzz->data[5] = 0;
	buzz->bytes = 6;

	buzz->duty = duty;
	buzz->duration = duration;
	buzz->delay = delay;
	
	INC_N_WRAP_PTR(buzz_head);

	return 0;
}

static int PWMPutBuzz(u_char duty, int delay)
{
	struct buzz *buzz;
	
	if (pwm_has_buzzes() >= (PWM_BUFSIZE - 1))
		return -1;	
	if (delay < min_delay) 
		delay = min_delay;
	
	buzz = &buzzes[buzz_head];
	
	buzz->data[0] = (PWM_WORD_TYPE_DUTY << 4);
	buzz->data[1] = duty;
	buzz->bytes = 2;

	buzz->duty = duty;
	buzz->duration = 0;
	buzz->delay = delay;
	
	INC_N_WRAP_PTR(buzz_head);
		
	return 0;
}

static int PWMPutSweep(u_char d1, u_char d2, int duration)
{
	struct buzz *buzz;
	short dd; 
	int delay;
	int steps, stepsize;
	int dir;
	int i, max_buzzes;

	if (duration < min_delay) return 0;
	if (!(dd = d2 - d1)) return 0;

	max_buzzes = PWM_BUFSIZE - pwm_has_buzzes() - 2;
	if (max_buzzes > max_steps) max_buzzes = max_steps - 1;
	if (max_buzzes <= 0) return 0;

	dir = (dd < 0) ? -1 : 1;
	steps = dd * dir;

	if (steps > max_buzzes) {
		stepsize = steps / max_buzzes;
		/* compensate for rounding errors */
		steps = max_buzzes - (((max_buzzes * stepsize) - steps) / stepsize); 
	} else {
		stepsize = 1;
	}
		
	delay = (duration / steps);

	if (delay < min_delay) {
		steps = duration / min_delay;
		stepsize = (dd * dir) / steps;
		/* compensate for rounding errors */
		steps += ((dd * dir) - (steps * stepsize)) / stepsize;
		delay = min_delay;
	}
	
	dbg("steps: %d, stepsize: %d, max_buzzes: %d, delay: %d", steps, stepsize, max_buzzes, delay);

	for (i=0;i<=steps;i++,d1+=(stepsize * dir)) {
		PWMPutBuzzTime(d1, delay, delay);
	}

	/* shut up */
	PWMPutBuzzTime(0, 1, min_delay);
	
	return 0;
}

static int pwm_buzz_handler(const char *path, const char *types, lo_arg **argv, 
			      int argc, lo_message msg, void *user_data)
{
	switch(argc) {
		case 1:
			PWMPutBuzz((u_char)argv[0]->i, 1000);
			break;
		case 2:
			PWMPutBuzzTime((u_char)argv[0]->i, (u_short)argv[1]->i, 1000);
			break;
		default: 
			warn(MODULE_NAME, "pwm buzz: wrong amount of arguments");
	}
	sios_add_source_ctx(&dev_pwm_buzz_src);
	return 0;
}

static int pwm_buzz_sweep_handler(const char *path, const char *types, lo_arg **argv, 
			      	int argc, lo_message msg, void *user_data)
{
	dbg("buzz sweep");
	switch(argc) {
		case 3:
			PWMPutSweep((u_char)argv[0]->i, (u_char)argv[1]->i, argv[2]->i);
			break;
		default: 
			warn(MODULE_NAME, "pwm buzz: wrong amount of arguments");
	}
	sios_add_source_ctx(&dev_pwm_buzz_src);
	return 0;
}

static int dev_pwm_buzz_write(struct sios_source_ctx * ctx, enum sios_event_type action) 
{
	struct buzz *buzz;
	size_t bytes = 0;
	int retval;
	u_char out[7];

	if (action != SIOS_EVENT_WRITE || !pwm_has_buzzes())
		return 0;

	buzz = &buzzes[buzz_tail];
	retval = write(ctx->fd, buzz->data, buzz->bytes);
	if (retval < 0) 
		err(MODULE_NAME, "write error '%s': %s", pwm_device, strerror(errno));

	INC_N_WRAP_PTR(buzz_tail);

	if (pwm_has_buzzes())
		ctx->period = buzzes[buzz_tail].delay * 1000;

	return !pwm_has_buzzes();
}

static int open_pwm_dev(const char * dev)
{
	u_char word[2];
	int n;

	int fd = open(dev, O_WRONLY | O_NONBLOCK);
	if (fd < 0) {
		err(MODULE_NAME, "error opening '%s': %s", dev, strerror(errno));
		return -1;
	}

	// set PWM-frequency for Buzz once. it remains set throughout.
	word[0] = ((PWM_WORD_TYPE_FREQ << 4) | ((PWM_BUZZ_FREQ & 0x0F00) >> 8));
	word[1] = (PWM_BUZZ_FREQ & 0x00FF);
	
	n = write(fd, word, 2);
	
	if (n < 0) {
		err(MODULE_NAME, "error setting buzz frequency: '%s'", dev);
		close(fd);
		return -1;
	}

	return fd;
}

static void close_pwm_dev(int fd)
{
	close(fd);
}

struct sios_method_desc osc_methods[] = {
	METHOD_DESC_INITIALIZER("buzz", "", NULL, pwm_buzz_handler, "put buzz"),
	METHOD_DESC_INITIALIZER("sweep", "", NULL, pwm_buzz_sweep_handler, "put sweep buzz"),
};

int pwm_buzz_init(void)
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

	dev_pwm_buzz_src.self = THIS_MODULE;
	dev_pwm_buzz_src.fd = fd;
	dev_pwm_buzz_src.period = min_delay * 1000;
	return 0;
}


void pwm_buzz_exit(void)
{
	sios_del_source_ctx(&dev_pwm_buzz_src);
	close_pwm_dev(dev_pwm_buzz_src.fd);
	sios_object_deregister(THIS_MODULE);
}

module_init(pwm_buzz_init)
module_exit(pwm_buzz_exit)
