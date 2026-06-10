/*
    IE CAMD driver transmit loop, extracted for host unit testing.

    Plain C only — no AROS headers, no ASM/REG/SAVEDS — so ie_test.c can
    compile it standalone with the host compiler. ie.c wraps it in the
    AROS-ABI ActivateXmit.
*/

#ifndef IE_XMIT_H
#define IE_XMIT_H

/*
    Pump MIDI bytes from a CAMD transmit source to an emit sink.

    next(ud) returns the next byte to transmit, or 0x100 when there are no
    more bytes (the CAMD transmitfunc contract). Every byte below the
    sentinel is passed to emit() verbatim, preserving MIDI running status.
*/
static inline void ie_pump_midi(
    unsigned long (*next)(void *ud), void *ud,
    void (*emit)(unsigned char b, void *eud), void *eud)
{
    unsigned long b;
    while ((b = next(ud)) != 0x100UL)
        emit((unsigned char)b, eud);
}

#endif /* IE_XMIT_H */
