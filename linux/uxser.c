/*
 * struct serial_t * back end (Unix-specific).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>

#include "serial_handle.h"

int serial_configure(struct serial_t * serial, struct serial_conf_t *conf)
{
	struct termios options;
	int bflag;

	if (serial->fd < 0)
		return SER_ECFG_OPEN;
	if (tcgetattr(serial->fd, &options) != 0)
		return SER_ECFG_GETATTR;

#define SETBAUD(x) (bflag = B ## x)
#define CHECKBAUD(x) \
	do { \
		if (conf->baudrate >= x) \
			SETBAUD(x); \
	} while (0)

	SETBAUD(50);
#ifdef B75
	CHECKBAUD(75);
#endif
#ifdef B110
	CHECKBAUD(110);
#endif
#ifdef B134
	CHECKBAUD(134);
#endif
#ifdef B150
	CHECKBAUD(150);
#endif
#ifdef B200
	CHECKBAUD(200);
#endif
#ifdef B300
	CHECKBAUD(300);
#endif
#ifdef B600
	CHECKBAUD(600);
#endif
#ifdef B1200
	CHECKBAUD(1200);
#endif
#ifdef B1800
	CHECKBAUD(1800);
#endif
#ifdef B2400
	CHECKBAUD(2400);
#endif
#ifdef B4800
	CHECKBAUD(4800);
#endif
#ifdef B9600
	CHECKBAUD(9600);
#endif
#ifdef B19200
	CHECKBAUD(19200);
#endif
#ifdef B38400
	CHECKBAUD(38400);
#endif
#ifdef B57600
	CHECKBAUD(57600);
#endif
#ifdef B76800
	CHECKBAUD(76800);
#endif
#ifdef B115200
	CHECKBAUD(115200);
#endif
#ifdef B153600
	CHECKBAUD(153600);
#endif
#ifdef B230400
	CHECKBAUD(230400);
#endif
#ifdef B307200
	CHECKBAUD(307200);
#endif
#ifdef B460800
	CHECKBAUD(460800);
#endif
#ifdef B500000
	CHECKBAUD(500000);
#endif
#ifdef B576000
	CHECKBAUD(576000);
#endif
#ifdef B921600
	CHECKBAUD(921600);
#endif
#ifdef B1000000
	CHECKBAUD(1000000);
#endif
#ifdef B1152000
	CHECKBAUD(1152000);
#endif
#ifdef B1500000
	CHECKBAUD(1500000);
#endif
#ifdef B2000000
	CHECKBAUD(2000000);
#endif
#ifdef B2500000
	CHECKBAUD(2500000);
#endif
#ifdef B3000000
	CHECKBAUD(3000000);
#endif
#ifdef B3500000
	CHECKBAUD(3500000);
#endif
#ifdef B4000000
	CHECKBAUD(4000000);
#endif
#undef CHECKBAUD
#undef SETBAUD
	cfsetispeed(&options, bflag);
	cfsetospeed(&options, bflag);

	options.c_cflag &= ~CSIZE;
	switch (conf->databits) {
	case 5:
		options.c_cflag |= CS5;
		break;
	case 6:
		options.c_cflag |= CS6;
		break;
	case 7:
		options.c_cflag |= CS7;
		break;
	case 8:
		options.c_cflag |= CS8;
		break;
	default:
		return SER_ECFG_DATABITS;
	}

	if (conf->stopbits == SER_STOPBITS_TWO)
		options.c_cflag |= CSTOPB;
	else
		options.c_cflag &= ~CSTOPB;

	options.c_cflag |= PARENB;
	if (conf->parity == SER_PAR_ODD)
		options.c_cflag |= PARODD;
	else if (conf->parity == SER_PAR_EVEN)
		options.c_cflag &= ~PARODD;
	else
		options.c_cflag &= ~PARENB;

	options.c_iflag &= ~(IXON | IXOFF);
#ifdef CRTSCTS
	options.c_cflag &= ~CRTSCTS;
#endif
#ifdef CNEW_RTSCTS
	options.c_cflag &= ~CNEW_RTSCTS;
#endif

	if (conf->flow == SER_FLOW_XONXOFF)
		options.c_iflag |= IXON | IXOFF;
	else if (conf->flow == SER_FLOW_RTSCTS) {
#ifdef CRTSCTS
		options.c_cflag |= CRTSCTS;
#endif
#ifdef CNEW_RTSCTS
		options.c_cflag |= CNEW_RTSCTS;
#endif
	}

	options.c_cflag |= CLOCAL | CREAD;
	options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
	options.c_iflag &= ~(ISTRIP | IGNCR | INLCR | ICRNL
#ifdef IUCLC
		| IUCLC
#endif
		);
	options.c_oflag &= ~(OPOST
#ifdef ONLCR
		| ONLCR
#endif
#ifdef OCRNL
		| OCRNL
#endif
#ifdef ONOCR
		| ONOCR
#endif
#ifdef ONLRET
		| ONLRET
#endif
		);
	options.c_cc[VMIN] = 0;
	options.c_cc[VTIME] = 1;

	if (tcsetattr(serial->fd, TCSANOW, &options) < 0)
		return SER_ECFG_SETATTR;

	return SER_CFG_DONE;
}

struct serial_t *create_serial(struct serial_conf_t *conf)
{
	struct serial_t * serial;
	void *pret;
	int len;

	len = strlen(conf->dev) + 1;
	serial = (struct serial_t *)malloc(sizeof(struct serial_t)
			+ len);
	if (serial == NULL)
		goto create_serial_exit_with_error_L0;
	serial->fd = open(conf->dev, O_RDWR | O_NOCTTY | O_NDELAY | O_NONBLOCK);
	if (serial->fd < 0)
		goto create_serial_exit_with_error_L1;
	strncpy(serial->name, conf->dev, len);
	if (serial_configure(serial, conf) != SER_CFG_DONE)
		goto create_serial_exit_with_error_L2;
	pret = (void *)serial_handle_input_init(serial, serial_handle_input_func,
			conf->ibufsize);
	if (pret == NULL)
		goto create_serial_exit_with_error_L2;
	pret = serial_handle_output_init(serial, serial_handle_output_func,
			conf->obufsize);
	if (pret == NULL) {
		free(serial->in);
create_serial_exit_with_error_L2:
		close(serial->fd);
create_serial_exit_with_error_L1:
		free(serial);
create_serial_exit_with_error_L0:
		return NULL;
	}

	return serial;
}

void serial_close(struct serial_t * serial)
{
	if (serial) {
		if (serial->in)
			free(serial->in);
		if (serial->out)
			free(serial->out);
		if (serial->fd >= 0) {
			close(serial->fd);
			serial->fd = -1;
		}
		free(serial);
	}
}

struct serial_handle_input *serial_handle_input_init(struct serial_t *serial,
		serial_input_callback_fn callback, int bufsize)
{
	if (serial == NULL)
		return NULL;
	serial->in = (struct serial_handle_input *)malloc(sizeof(*serial->in)
			+ bufsize);
	if (serial->in == NULL)
		return NULL;
	serial->in->fd = STDOUT_FILENO;
	serial->in->done = false;
	serial->in->busy = false;
	serial->in->defunct = false;
	serial->in->moribund = false;
	serial->in->readerr = 0;
	serial->in->handle_size = 0;
	serial->in->handled_size = 0;
	serial->in->bufsize = bufsize;
	serial->in->callback = callback;
	serial->in->super = (void *)serial;

	return serial->in;
}

struct serial_handle_output *serial_handle_output_init(struct serial_t *serial,
		serial_output_callback_fn callback, int bufsize)
{
	if (serial == NULL)
		return NULL;
	serial->out = (struct serial_handle_output *)malloc(sizeof(*serial->out)
			+ bufsize);
	if (serial->out == NULL)
		return NULL;
	serial->out->fd = STDIN_FILENO;
	serial->out->busy = false;
	serial->out->done = false;
	serial->out->defunct = false;
	serial->out->moribund = false;
	serial->out->writeerr = 0;
	serial->out->handle_size = 0;
	serial->out->handled_size = 0;
	serial->out->bufsize = bufsize;
	serial->out->callback = callback;
	serial->out->super = (void *)serial;

	return serial->out;
}
