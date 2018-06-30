#ifndef __SERIAL_H_
#define __SERIAL_H_

#include <stdbool.h>

#if defined(__WIN32)
#include <windows.h>
#endif

struct serial_handle_input;
struct serial_handle_output;

#if defined(__WIN32)
typedef DWORD (* serial_input_callback_fn)(struct serial_handle_input *);
typedef DWORD (* serial_output_callback_fn)(struct serial_handle_output *);
#elif defined(__linux__)
typedef int (* serial_input_callback_fn)(struct serial_handle_input *);
typedef int (* serial_output_callback_fn)(struct serial_handle_output *);
#endif

struct serial_handle_input {
#if defined(__WIN32)
	HANDLE h;           	/* the handle itself */
	HANDLE ev_to_main;  	/* event used to signal main thread */
	HANDLE ev_from_main;	/* event used to signal back to us */
	
#define HANDLE_FLAG_OVERLAPPED	0x01U
#define HANDLE_FLAG_UNITBUFFER	0x02U
#define HANDLE_FLAG_IGNOREEOF 	0x04U
	BYTE flags;          	/* Data set at initialisation and then read-only */
#elif defined(__linux__)
	int fd;
	int signal_to_main;
	int signal_from_main;
#endif
	bool moribund; 	/* are we going to kill this soon? */
	bool done;     	/* request subthread to terminate */
	bool defunct;  	/* has the subthread already gone? */
	bool busy;     	/* operation currently in progress? */
	void *super;	/* for client to remember who they are */
	int readerr;   	/* lets us know about read errors */
	serial_input_callback_fn callback;
#if defined(__WIN32)
	DWORD handle_size; 	/* how much data that was */
	DWORD handled_size;	/* how much data was read */
	DWORD bufsize;
#elif defined(__linux__)
	int handle_size;
	int handled_size;
	int bufsize;
#endif
	char buffer[]; 	/* Data set by the input thread, and read by the main thread. */
};

struct serial_handle_output {
#if defined(__WIN32)
	HANDLE h;           	/* the handle itself */
	HANDLE ev_to_main;  	/* event used to signal main thread */
	HANDLE ev_from_main;	/* event used to signal back to us */
	BYTE flags;          	/* Data set at initialisation and then read-only */
#elif defined(__linux__)
	int fd;
	int signal_to_main;
	int signal_from_main;
#endif
	bool moribund;      	/* are we going to kill this soon? */
	bool done;          	/* request subthread to terminate */
	bool defunct;       	/* has the subthread already gone? */
	bool busy;          	/* operation currently in progress? */
	void *super;     	/* for client to remember who they are */
    int writeerr;        	/* return value from WriteFile */
    //bufchain queued_data;	/* data still waiting to be written */
    serial_output_callback_fn callback;
#if defined(__WIN32)
	DWORD handle_size;
	DWORD handled_size;    	/* how much data we actually wrote */
	DWORD bufsize;
#elif defined(__linux__)
	int handle_size;
	int handled_size;
	int bufsize;
#endif
	char buffer[];
};

struct serial_t {
#if defined(__WIN32)
	HANDLE h;
#elif defined(__linux__) /* under linux */
	int fd;
#endif
	struct serial_handle_input *in;
	struct serial_handle_output *out;
	char name[]; /* the name of this serial device */
};

__attribute__((aligned(4))) struct serial_conf_t {
	const char *dev;
#if defined(__WIN32)
	DWORD ibufsize;
	DWORD obufsize;
#elif defined(__linux__)
	int ibufsize; /* input buffser byte size */
	int obufsize; /* output buffer byte size */
#endif
	int baudrate;
	char databits;
	enum {
		SER_FLOW_NONE    = 0x00U,
		SER_FLOW_XONXOFF = 0x01U,
		SER_FLOW_RTSCTS  = 0x02U,
		SER_FLOW_DSRDTR  = 0x03U
	} flow : 4;
	enum {
		SER_PAR_NONE = 0x00U,
		SER_PAR_ODD  = 0x01U,
		SER_PAR_EVEN = 0x02U,
		SER_PAR_MARK = 0x03U,
		SER_PAR_SPACE = 0x04U
	} parity : 4;
	enum {
		SER_STOPBITS_NONE = 0x00U,
		SER_STOPBITS_ONE  = 0x01U,
		SER_STOPBITS_ONE5 = 0x02U,
		SER_STOPBITS_TWO  = 0x03U
	} stopbits : 4;
};

/*
 * Creates a structure of serial.
 */
struct serial_t *create_serial(struct serial_conf_t *conf);

/*
 * Configs the serial device, the function will return the specified value
 * defined by under macros, returns 0 if configure is success.
 */
#define SER_CFG_DONE	 	0x0000U
#define SER_ECFG_GETATTR 	0x0001U
#define SER_ECFG_SETATTR 	0x0002U
#define SER_ECFG_OPEN		0x0003U
#define SER_ECFG_DATABITS	0x0004U
#define SER_ECFG_TIMEOUT 	0x0005U
int serial_configure(struct serial_t * serial, struct serial_conf_t *conf);

/*
 * Releases the resources and close serial port.
 */
void serial_close(struct serial_t * serial);

#if defined(__WIN32)
/*
 * Initializes the serial input handler, and the function will callback another
 * function to handle the input data from serial.
 *
 * note: Users should define the callback function by themselies.If there is
 * nothing need to do, then just set the pointer of callback function to NULL.
 */
struct serial_handle_input *serial_handle_input_init(struct serial_t *serial,
		serial_input_callback_fn callback, DWORD bufsize);

/*
 * This function's work is like serial_handle_input_init, but handles the data
 * output to serial.
 */
struct serial_handle_output *serial_handle_output_init(struct serial_t *serial,
		serial_output_callback_fn callback, DWORD bufsize);
#elif defined(__linux__)
struct serial_handle_input *serial_handle_input_init(struct serial_t *serial,
		serial_input_callback_fn callback, int bufsize);
struct serial_handle_output *serial_handle_output_init(struct serial_t *serial,
		serial_output_callback_fn callback, int bufsize);
#endif
		
#endif
