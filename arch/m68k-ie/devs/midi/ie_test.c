/*
    Host unit test for the IE CAMD driver transmit loop (ie_xmit.h).

    Builds standalone with the host compiler, no AROS headers:
        gcc -Wall -Wextra -o ie_test ie_test.c && ./ie_test

    Verifies ie_pump_midi() pulls bytes from the next-callback until the
    0x100 sentinel and emits each byte exactly once, in order, without
    emitting the sentinel or making any extra emit call.
*/

#include <stdio.h>
#include <string.h>

#include "ie_xmit.h"

struct mock_source {
    const unsigned long *bytes;
    unsigned long count;
    unsigned long pos;
};

static unsigned long mock_next(void *ud)
{
    struct mock_source *src = ud;
    if (src->pos >= src->count)
        return 0x100UL;
    return src->bytes[src->pos++];
}

struct capture {
    unsigned char buf[16];
    unsigned long len;
};

static void capture_emit(unsigned char b, void *eud)
{
    struct capture *cap = eud;
    if (cap->len < sizeof(cap->buf))
        cap->buf[cap->len] = b;
    cap->len++;
}

static int failures = 0;

static void check(int cond, const char *what)
{
    if (!cond) {
        fprintf(stderr, "FAIL: %s\n", what);
        failures++;
    }
}

int main(void)
{
    /* Note-on followed by the 0x100 "no more bytes" sentinel. */
    {
        const unsigned long stream[] = { 0x90, 0x3C, 0x64, 0x100 };
        struct mock_source src = { stream, 4, 0 };
        struct capture cap;
        memset(&cap, 0, sizeof(cap));

        ie_pump_midi(mock_next, &src, capture_emit, &cap);

        check(cap.len == 3, "emits exactly 3 bytes (sentinel not emitted, no extra write)");
        check(cap.len >= 3 && cap.buf[0] == 0x90 && cap.buf[1] == 0x3C && cap.buf[2] == 0x64,
              "emits exactly {0x90,0x3C,0x64} in order");
        check(src.pos == 4, "stopped at the sentinel, no reads past it");
    }

    /* Immediate sentinel: nothing to transmit, no emit call at all. */
    {
        const unsigned long stream[] = { 0x100 };
        struct mock_source src = { stream, 1, 0 };
        struct capture cap;
        memset(&cap, 0, sizeof(cap));

        ie_pump_midi(mock_next, &src, capture_emit, &cap);

        check(cap.len == 0, "immediate sentinel emits nothing");
    }

    if (failures) {
        fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    printf("ie_test: all checks passed\n");
    return 0;
}
