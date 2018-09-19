#include <stdio.h>
#include <conio.h>
#include "keymap.h"
#include "serial_handle.h"

DWORD serial_handle_input_func(struct serial_handle_input *in)
{
    BOOL writeret;

    writeret = WriteFile(in->h, in->buffer, in->handle_size, &in->handled_size,
            NULL);
    if (!writeret) {
        in->readerr = GetLastError();
        SetEvent(in->ev_to_main);
        WaitForSingleObject(in->ev_from_main, INFINITE);
    }

    return in->handled_size;
}

DWORD WINAPI serial_in_thread(void *param)
{
    OVERLAPPED ovl, *povl;
    HANDLE oev;
    int readret, readlen;
    struct serial_handle_input *in = (struct serial_handle_input *)param;

    if (in->flags & HANDLE_FLAG_OVERLAPPED) {
        povl = &ovl;
        oev = CreateEvent(NULL, TRUE, FALSE, NULL);
    } else
        povl = NULL;

    if (in->flags & HANDLE_FLAG_UNITBUFFER)
        readlen = 1;
    else
        readlen = in->bufsize;

    while (!in->moribund) {
        if (in->done) { /* set by output thread */
            SetEvent(in->ev_to_main);
            WaitForSingleObject(in->ev_from_main, INFINITE);
            continue;
        }
        if (povl) {
            memset(povl, 0, sizeof(OVERLAPPED));
            povl->hEvent = oev;
        }

        /* it looks like a stupid lock, may not be useful */
        while (((struct serial_t *)in->super)->out->busy)
            continue;
        in->busy = true;
        readret = ReadFile(((struct serial_t *)in->super)->h, in->buffer,
                readlen, &in->handle_size, povl);
        in->busy = false;

        if (!readret)
            in->readerr = GetLastError();
        else
            in->readerr = 0;
        if (povl && !readret && in->readerr == ERROR_IO_PENDING) {
            WaitForSingleObject(povl->hEvent, INFINITE);
            readret = GetOverlappedResult(((struct serial_t *)in->super)->h,
                    povl, &in->handle_size, FALSE);
            if (!readret)
                in->readerr = GetLastError();
            else
                in->readerr = 0;
        }

        if (!readret) {
            if (in->readerr == ERROR_BROKEN_PIPE)
                in->readerr = 0;
            in->handle_size = 0;
        }
        if (readret && in->handle_size == 0) {
            if (in->flags & HANDLE_FLAG_IGNOREEOF)
                continue; /* Skip EOF */
            else {
            /* EOF means the read operation is done, so just request to exit */
                in->done = true;
                SetEvent(in->ev_to_main);
                WaitForSingleObject(in->ev_from_main, INFINITE);
            }
        } else
            in->callback(in);
    }
    if (povl)
        CloseHandle(oev);
    /* tell main thread that subthread is already exit */
    in->defunct = true;
    SetEvent(in->ev_to_main);
    return 0;
}

DWORD serial_handle_output_func(struct serial_handle_output *out)
{
    BOOL readret;
    int ch;
    unsigned short size;

    if (out->h == GetStdHandle(STD_INPUT_HANDLE)){
        ch = _getwch(); /* also see _getwch() */
        *(int *)out->buffer = ch;
        out->handle_size = sizeof(char);
        /* a key to exit program */
        if (ch == 0x18) { /* should make input thread stop */
            out->done = true;
            out->handle_size = 0;
            /*
             * signal to stop input thread, and then clean the relative
             * buffer to make the function ReadFile quit, cause input
             * thread may be blocked by it.
             */
            ((struct serial_t *)out->super)->in->done = true;
            PurgeComm(((struct serial_t *)out->super)->h, PURGE_RXCLEAR |
                PURGE_RXABORT | PURGE_TXCLEAR | PURGE_TXABORT);
            goto serial_handle_output_func_to_main_event_L0;
        } else if (ch == 0xE0 || ch == 0x00) { /* Enter special key */
            size = keymap_find((ch << 8) + _getwch(), out->buffer);
            if (size == 0 || size > out->bufsize) {
                out->buffer[0] = 0;
                goto serial_handle_output_func_normal_return_L0;
            }
            out->handle_size = size;
        }
    } else {
        readret = ReadFile(out->h, out->buffer, out->bufsize, &out->handle_size,
                NULL);
        if (!readret) {
            out->writeerr = GetLastError();
serial_handle_output_func_to_main_event_L0:
            SetEvent(out->ev_to_main);
            WaitForSingleObject(out->ev_from_main, INFINITE);
        }
    }

serial_handle_output_func_normal_return_L0:
    return out->handle_size;
}

DWORD WINAPI serial_out_thread(void *param)
{
    struct serial_handle_output *out = (struct serial_handle_output *)param;
    OVERLAPPED ovl, *povl;
    HANDLE oev;
    int writeret;

    if (out->flags & HANDLE_FLAG_OVERLAPPED) {
        povl = &ovl;
        oev = CreateEvent(NULL, TRUE, FALSE, NULL);
    } else
        povl = NULL;

    while (!out->moribund) {
        if (povl) {
            memset(povl, 0, sizeof(OVERLAPPED));
            povl->hEvent = oev;
        }

        out->callback(out);
        while (((struct serial_t *)out->super)->in->busy)
            continue;
        out->busy = true;
        writeret = WriteFile(((struct serial_t *)out->super)->h, out->buffer,
                out->handle_size, &out->handled_size, povl);
        out->busy = false;

        /*
         * Trys to exit and don't set this statement before write,
         * cause WaitForSingleObject will blocks the input thread,
         * and then we should write exit code (^D) to serial port
         * before we close this thread.
         */
        if (out->done)
            continue;

        if (!writeret)
            out->writeerr = GetLastError();
        else
            out->writeerr = 0;
        if (povl && !writeret && GetLastError() == ERROR_IO_PENDING) {
            writeret = GetOverlappedResult(
                    ((struct serial_t *)out->super)->h, povl,
                    &out->handled_size, TRUE);
            if (!writeret)
                out->writeerr = GetLastError();
            else
                out->writeerr = 0;
        }
        if (!writeret) {
            out->done = true;
            SetEvent(out->ev_to_main);
            WaitForSingleObject(out->ev_from_main, INFINITE);
            continue;
        }
    }
    if (povl)
        CloseHandle(oev);
    out->defunct = true;
    SetEvent(out->ev_to_main);

    return 0;
}
