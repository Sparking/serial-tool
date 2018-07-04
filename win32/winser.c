#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "serial_handle.h"

void serial_close(struct serial_t *serial)
{
	if (serial) {
		if (serial->out) {
			free(serial->out);
			serial->out = NULL;
		}
		if (serial->in) {
			free(serial->in);
			serial->in = NULL;
		}
		if (serial->h != INVALID_HANDLE_VALUE) {
			ClearCommBreak(serial->h);
			CloseHandle(serial->h);
			serial->h = INVALID_HANDLE_VALUE;
			free(serial);
		}
	}
}

static void __dcb_reset_flow(DCB *dcb)
{
	dcb->fInX = FALSE;
	dcb->fOutX = FALSE;
	dcb->fNull = FALSE;
	dcb->fBinary = TRUE;
	dcb->fErrorChar = FALSE;
	dcb->fOutxCtsFlow = FALSE;
	dcb->fOutxDsrFlow = FALSE;
	dcb->fAbortOnError = FALSE;
	dcb->fDsrSensitivity = FALSE;
	dcb->fTXContinueOnXoff = FALSE;
	dcb->fDtrControl = DTR_CONTROL_ENABLE;
	dcb->fRtsControl = RTS_CONTROL_ENABLE;
}

int serial_configure(struct serial_t *serial, struct serial_conf_t *conf)
{
    DCB dcb;
    COMMTIMEOUTS timeouts;

    if (GetCommState(serial->h, &dcb)) {
		/*
		 * Boilerplate.
		 */
		__dcb_reset_flow(&dcb);
		/*
		* Configurable parameters.
		*/
		dcb.BaudRate = conf->baudrate;
		dcb.ByteSize = conf->databits;
		switch (conf->stopbits) { /* ONESTOPBIT, ONE5STOPBITS, TWOSTOPBITS */
		case SER_STOPBITS_ONE:
			dcb.StopBits = ONESTOPBIT;
			break;
		case SER_STOPBITS_ONE5:
			dcb.StopBits = ONE5STOPBITS;
			break;
		case SER_STOPBITS_TWO:
			dcb.StopBits = TWOSTOPBITS;
			break;
		default:
			break;
		}
		/* NOPARITY, ODDPARITY, EVENPARITY, MARKPARITY, SPACEPARITY */
		switch (conf->parity) {
		case SER_PAR_NONE:
			dcb.Parity = NOPARITY;
			break;
		case SER_PAR_ODD:
			dcb.Parity = ODDPARITY;
			break;
		case SER_PAR_EVEN:
			dcb.Parity = EVENPARITY;
			break;
		case SER_PAR_MARK:
			dcb.Parity = MARKPARITY;
			break;
		case SER_PAR_SPACE:
			dcb.Parity = SPACEPARITY;
			break;
		}
		switch (conf->flow) {
		case SER_FLOW_NONE:
			break;
		case SER_FLOW_XONXOFF:
			dcb.fOutX = dcb.fInX = TRUE;
			break;
		case SER_FLOW_RTSCTS:
			dcb.fRtsControl = RTS_CONTROL_HANDSHAKE;
			dcb.fOutxCtsFlow = TRUE;
			break;
		case SER_FLOW_DSRDTR:
			dcb.fDtrControl = DTR_CONTROL_HANDSHAKE;
			dcb.fOutxDsrFlow = TRUE;
			break;
		}
	
		if (!SetCommState(serial->h, &dcb))
			return SER_ECFG_SETATTR;
	
		timeouts.ReadIntervalTimeout = 1;
		timeouts.ReadTotalTimeoutMultiplier = 0;
		timeouts.ReadTotalTimeoutConstant = 0;
		timeouts.WriteTotalTimeoutMultiplier = 0;
		timeouts.WriteTotalTimeoutConstant = 0;
		if (!SetCommTimeouts(serial->h, &timeouts))
			return SER_ECFG_TIMEOUT;
		return SER_CFG_DONE;
    }

    return SER_ECFG_GETATTR;
}

static unsigned int _serial_checkport(const char *serline, unsigned int *width)
{
	int len, port, port_pos;
	
	len = strlen(serline);
	port = 0;
	if (len < 4)
		return 0;
	if (strncmp(serline, "COM", 3) == 0)
		port_pos = 3;
	else if (strncmp(serline, "\\\\.\\COM", 7) == 0)
		port_pos = 7;
	else
		return 0;
	while (port_pos < len) {
		if (serline[port_pos] < '0' || serline[port_pos] > '9')
			return 0;
		port = port * 10 + serline[port_pos] - '0';
		++port_pos;
	}

	return port;
}

static struct serial_t * _serial_init_name(const char *sername)
{
	char name[16];
	unsigned int j, i;
	struct serial_t *serial;

	j = strlen(sername);
	for (i = 0; i <= j; i++) {
		if (sername[i] <= 'z' && sername[i] >= 'a')
			name[i] = sername[i] + 'A' - 'a';
		else
			name[i] = sername[i];
	}
	j = _serial_checkport(name, &i);
	if (j != 0) {
		i += 8;
		serial = (struct serial_t *)malloc(sizeof(struct serial_t) + i);
		if (serial)
			sprintf(serial->name, "\\\\.\\COM%u", j);
	} else
		serial = NULL;

	return serial;
}


struct serial_t *create_serial(struct serial_conf_t *conf)
{
	struct serial_t *serial;

	serial = _serial_init_name(conf->dev);
	if (serial == NULL)
		return NULL;
	serial->h = CreateFile(serial->name, GENERIC_READ | GENERIC_WRITE, 0, NULL,
			OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
	if (serial->h == INVALID_HANDLE_VALUE)
		goto create_serial_cleanup_exit_L0;
	if (serial_configure(serial, conf) != SER_CFG_DONE)
		goto create_serial_cleanup_exit_L1;

	if (serial_handle_input_init(serial, serial_handle_input_func,
			conf->ibufsize) == NULL)
		goto create_serial_cleanup_exit_L2;
	if (serial_handle_output_init(serial, serial_handle_output_func,
			conf->obufsize) == NULL) {
create_serial_cleanup_exit_L2:
		free(serial->in);
create_serial_cleanup_exit_L1:
		CloseHandle(serial->h);
create_serial_cleanup_exit_L0:
		free(serial);
		return NULL;
	}

	PurgeComm(serial->h, PURGE_RXCLEAR | PURGE_RXABORT | PURGE_TXCLEAR |
			PURGE_TXABORT);
	return serial;
}

struct serial_handle_output *serial_handle_output_init(struct serial_t *serial,
		serial_output_callback_fn callback, DWORD bufsize)
{
	serial->out = (struct serial_handle_output *)malloc(
			sizeof(struct serial_handle_output) + bufsize);
	if (serial->out == NULL)
		return NULL;
	serial->out->bufsize = bufsize;
	serial->out->busy = false;
	serial->out->done = false;
	serial->out->defunct = false;
	serial->out->moribund = false;
	serial->out->super = serial;
	serial->out->callback = callback;
	serial->out->writeerr = 0;
	serial->out->handle_size = 0;
	serial->out->handled_size = 0;
	serial->out->h = GetStdHandle(STD_INPUT_HANDLE);
	serial->out->flags = HANDLE_FLAG_OVERLAPPED;
	serial->out->ev_to_main = CreateEvent(NULL, FALSE, FALSE, NULL);
	serial->out->ev_from_main = CreateEvent(NULL, FALSE, FALSE, NULL);

	return serial->out;
}

struct serial_handle_input *serial_handle_input_init(struct serial_t *serial,
		serial_input_callback_fn callback, DWORD bufsize)
{
	serial->in = (struct serial_handle_input *)malloc(
			sizeof(struct serial_handle_input) + bufsize);
	if (serial->in == NULL)
		return NULL;
	serial->in->flags = HANDLE_FLAG_OVERLAPPED |
			HANDLE_FLAG_IGNOREEOF;
	serial->in->bufsize = bufsize;
	serial->in->busy = false;
	serial->in->done = false;
	serial->in->defunct = false;
	serial->in->moribund = false;
	serial->in->super = serial;
	serial->in->callback = callback;
	serial->in->readerr = 0;
	serial->in->handle_size = 0;
	serial->in->handled_size = 0;
	serial->in->h = GetStdHandle(STD_OUTPUT_HANDLE);
	serial->in->ev_to_main = CreateEvent(NULL, FALSE, FALSE, NULL);
	serial->in->ev_from_main = CreateEvent(NULL, FALSE, FALSE, NULL);

	return serial->in;
}
