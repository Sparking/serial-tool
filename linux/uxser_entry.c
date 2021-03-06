#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <pthread.h>

#include "serial_handle.h"

void *serial_read_thread(void *param)
{
    pthread_t in_tid;
    struct serial_t *serial = (struct serial_t *)param;

    if (pthread_create(&in_tid, NULL, serial_in_thread, param))
        return NULL;
    while (!serial->in->defunct) {
        while (serial->in->signal_to_main)
            usleep(1000);
        serial->in->signal_to_main = false;
        if (serial->in->done) {
            serial->in->moribund = true;
            serial->in->signal_from_main = true;
            pthread_join(in_tid, NULL);
            break;
        }
        usleep(1000);
    }
    return NULL;
}

void *serial_write_thread(void *param)
{
    int retry;
    struct serial_t *serial = (struct serial_t *)param;
    pthread_t out_tid;

    if (pthread_create(&out_tid, NULL, serial_out_thread, param))
        return NULL;
    retry = 0;
    while (!serial->out->defunct) { /* subthread is still running */
        while (!serial->out->signal_to_main)
            usleep(1000);
        serial->out->signal_to_main = false;

        if (serial->out->writeerr != 0) {
            if (++retry == 5)
                serial->out->moribund = true;
        } else
            retry = 0;

        /* just let subthread exit if there is nothing to do. */
        if (serial->out->done) {
            serial->out->moribund = true;
            serial->out->signal_from_main = true;
            pthread_join(out_tid, NULL);
            break;
        }
        serial->out->signal_from_main = true;
    }
    serial->in->moribund = true; /* force read threads exit */
    pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
    struct serial_t *serial;
    struct serial_conf_t conf;
    pthread_t read_tid, write_tid;

    if (argc != 3) {
        fprintf(stderr, "Usage:\n%s <device> <baudrate>\n", argv[0]);
        return -1;
    }

    conf.dev = argv[1];
    conf.baudrate = atoi(argv[2]);
    conf.databits = 8;
    conf.stopbits = SER_STOPBITS_ONE;
    conf.parity = SER_PAR_NONE;
    conf.flow = SER_FLOW_NONE;
    conf.ibufsize = 4096;
    conf.obufsize = 4096;
    serial = create_serial(&conf);
    if (serial == NULL) {
        fprintf(stderr, "open %s failed\n", conf.dev);
        return -1;
    }

    if (pthread_create(&read_tid, NULL, serial_read_thread, (void *)serial) != 0) {
        fprintf(stderr, "create read thread failed.\n");
        return -1;
    }
    if (pthread_create(&write_tid, NULL, serial_write_thread, (void *)serial) != 0) {
        fprintf(stderr, "create write thread failed.\n");
        return -1;
    }
    pthread_join(write_tid, NULL);
    pthread_join(read_tid, NULL);
    serial_close(serial);
    putchar('\n');

    return 0;
}

