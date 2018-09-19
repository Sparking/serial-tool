#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "serial_handle.h"

static DWORD WINAPI serial_write_thread(void *param)
{
    int retry;
    DWORD subthread_id;
    HANDLE subthread_handler;
    struct serial_t *serial;
    
    serial = (struct serial_t *)param;
    subthread_handler = CreateThread(NULL, 0, serial_out_thread, serial->out, 0,
            &subthread_id);
    retry = 0;
    while (!serial->out->defunct) {
        WaitForSingleObject(serial->out->ev_to_main, INFINITE);

        /* responds the exit signal */
        if (serial->out->done) {
serial_write_thread_subthread_moribund_L0:
            serial->out->moribund = true; /* needs to kill subthread */
            SetEvent(serial->out->ev_from_main);
            /* waits for the flag out->defunct */
            WaitForSingleObject(serial->out->ev_to_main, INFINITE);
            continue;
        }

        /* occurs error during output operation */
        if (serial->out->writeerr) {
            if (++retry > 5)
                goto serial_write_thread_subthread_moribund_L0;
            continue;
        }
        retry = 0;
    }
    WaitForSingleObject(subthread_handler, INFINITE);

    return 0;
}

static DWORD WINAPI serial_read_thread(void *param)
{
    DWORD subthread_id;
    HANDLE subthread_handler;
    struct serial_t *serial;
    
    serial = (struct serial_t *)param;
    subthread_handler = CreateThread(NULL, 0, serial_in_thread, serial->in, 0,
            &subthread_id);
    while (!serial->in->defunct) {
        WaitForSingleObject(serial->in->ev_to_main, INFINITE);
        if (serial->in->done || serial->in->readerr) {
            serial->in->moribund = true;
            SetEvent(serial->in->ev_from_main);
            WaitForSingleObject(serial->in->ev_to_main, INFINITE);
            continue;
        }
    }
    WaitForSingleObject(subthread_handler, INFINITE);
    putchar('\n');
    return 0;
}

int main(int argc, char *argv[])
{
    DWORD out_tid, in_tid;
    struct serial_t *serial;
    struct serial_conf_t conf;
    HANDLE out_thread_handler, in_thread_handler;

    if (argc != 3) {
        fprintf(stderr, "Usage:\n\t%s <port> <baudrate>\n", argv[0]);
        return -1;
    }
    
    conf.dev = argv[1];
    conf.baudrate = atoi(argv[2]); /* 115200 */
    conf.databits = 8;
    conf.stopbits = SER_STOPBITS_ONE;
    conf.parity = SER_PAR_NONE;
    conf.flow = SER_FLOW_NONE;
    conf.obufsize = 8; /* size of a charactor */
    conf.ibufsize = 4096;
    serial = create_serial(&conf);
     if (serial == NULL) {
        fprintf(stderr, "failed to use port %s for some reasons\n", conf.dev);
        return -1;
    }
    in_thread_handler = CreateThread(NULL, 0, serial_read_thread,
            (void *)serial, 0, &in_tid);
    out_thread_handler = CreateThread(NULL, 0, serial_write_thread,
            (void *)serial, 0, &out_tid);
    WaitForSingleObject(in_thread_handler, INFINITE);
    WaitForSingleObject(out_thread_handler, INFINITE);
    serial_close(serial);

    return 0;
}
