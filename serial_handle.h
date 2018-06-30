#ifndef __SERIAL_HANDLE_H_
#define __SERIAL_HANDLE_H_

#include "serial.h"

#if defined(WIN32)
DWORD serial_handle_output_func(struct serial_handle_output *out);
DWORD serial_handle_input_func(struct serial_handle_input *in);
DWORD WINAPI serial_out_thread(void *param);
DWORD WINAPI serial_in_thread(void *param);
#elif defined(__linux__)
int serial_handle_output_func(struct serial_handle_output *out);
int serial_handle_input_func(struct serial_handle_input *in);
void *serial_out_thread(void *param);
void *serial_in_thread(void *param);
#endif

#endif
