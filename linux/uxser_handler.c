#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <termios.h>

#include "serial_handle.h"

int serial_handle_output_func(struct serial_handle_output *out)
{
    int handled_size;

    handled_size = write(((struct serial_t *)out->super)->fd, out->buffer,
            out->handle_size);
    out->handle_size -= handled_size;
    out->handled_size = handled_size;

    return handled_size;
}

int serial_handle_input_func(struct serial_handle_input *in)
{
    return write(in->fd, in->buffer, in->handle_size);
}

void *serial_out_thread(void *param)
{
    char *buffer;
    int size, read_size;
    struct termios prev_tty_options;
    struct termios current_tty_options;
    struct serial_t *serial = (struct serial_t *)param;

    tcgetattr(STDIN_FILENO, &prev_tty_options);
    current_tty_options = prev_tty_options;
    current_tty_options.c_lflag &= ~(ICANON | ISIG);
    current_tty_options.c_lflag &= ~ECHO;
    current_tty_options.c_cc[VTIME] = 0;
    current_tty_options.c_cc[VMIN] = 1;
    tcsetattr(STDIN_FILENO, TCSANOW, &current_tty_options);

    while (!serial->out->moribund) { /* is continue ? */
        /* the input thread is running and this thread should wait */
        serial->out->callback(serial->out);
        size = 0;
        while (size < serial->out->bufsize) {
            buffer = serial->out->buffer + size;
            read_size = read(serial->out->fd, buffer, sizeof(int));
            serial->out->handle_size += read_size;
            if (*buffer == 24 && read_size == 1) { /* Ctrl-x to exit*/
                /* tell main thread that subthread wants exit */
                serial->out->done = true;
                /*
                 * send signal to tell main thread, and main thread should
                 * respond this signal with setting some flags and send a
                 * signal back, then subthread just needs to wait this signal
                 * and to respond it.
                 * After responding the signal, thread should clean it right now.
                 */
                serial->out->signal_to_main = true;
                while (!serial->out->signal_from_main)
                    usleep(1000);
                serial->out->signal_from_main = false; /* clean the signal */
                /* respond main thread to kill this thread */
                if (serial->out->moribund)
                    break;
                else /* may main thread doesn't want subthread to exit */
                    serial->out->done = false;
            }

            if (serial->out->fd == STDIN_FILENO)
                break;
            /* encounters the special character and send buffer right now */
            if (((*(char *)buffer < 32  || *(char *)buffer >= 127) &&
                    read_size == 1) || read_size > 1)
                break;
            size += read_size;
        }
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &prev_tty_options);
    serial->out->defunct = true;

    pthread_exit(NULL);
}

void *serial_in_thread(void *param)
{
    struct serial_t *serial = (struct serial_t *)param;

    while (!serial->in->moribund) {
        /*
         * it's possible the block problem will not kill subthread, so should
         * set the member c_cc[VMIN] of structure <struct termios> to zero.
         */
        serial->in->handle_size = read(serial->fd, serial->in->buffer,
                serial->in->bufsize);
        /*
         * Somwthing to do.
         */
        while (serial->in->signal_from_main)
            usleep(1000);
        serial->in->signal_from_main = false;

        if (serial->in->moribund) {
            break;
        }
        serial->in->callback(serial->in);
    }
    serial->in->signal_to_main = true;
    serial->in->defunct = true;

    pthread_exit(NULL);
}

