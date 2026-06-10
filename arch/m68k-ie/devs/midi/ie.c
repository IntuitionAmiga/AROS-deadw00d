/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: CAMD leaf driver for the Intuition Engine live-MIDI MMIO port.
*/

/*********************************************************************************
  Output-only CAMD MIDI driver for the Intuition Engine (IE).

  IE exposes a generic live-MIDI port as three byte-wide MMIO registers; the
  transmit path is a single volatile byte store of each raw MIDI byte to
  IE_MIDI_LIVE_DATA. The store itself crosses into the host's MIDI synth, so
  no interrupt, task or hardware FIFO handling is needed: ActivateXmit can
  drain the CAMD transmit function inline.

  The transmit loop lives in ie_xmit.h (plain C, host-unit-tested by
  ie_test.c); this file is only the AROS/CAMD ABI boilerplate around it.

  IMPORTANT: this driver stores to fixed IE MMIO addresses with no runtime
  probe (there is no safe way to detect the port on foreign hardware), so it
  is built only for the ie-m68k target via arch/m68k-ie/devs/midi. Never move
  it into the target-generic workbench/devs/midi build.
***********************************************************************************/

#include <proto/exec.h>
#include <exec/types.h>
#include <midi/camddevices.h>
#include <libcore/compiler.h>

#include "ie_xmit.h"

/* IE live-MIDI MMIO registers (bare-metal m68k: plain address-space stores). */
#define IE_MIDI_LIVE_DATA 0xF0BF4UL /* W: raw MIDI byte (running-status stream) */
#define IE_MIDI_LIVE_CTRL 0xF0BF6UL /* W: bit 0 = all notes off + parser reset */

struct ExecBase *SysBase;

int main(void){
  /* A camd mididriver is not supposed to be run directly, so we return an error. */
  return -1;
}

/*    Prototypes    */

BOOL ASM Init(REG(a6) APTR sysbase);
void Expunge(void);
SAVEDS ASM struct MidiPortData *OpenPort(
                                         REG(a3) struct MidiDeviceData *data,
                                         REG(d0) LONG portnum,
                                         REG(a0) ULONG (* ASM transmitfunc)(APTR REG(a2) userdata),
                                         REG(a1) void (* ASM receivefunc)(UWORD REG(d0) input,APTR REG(a2) userdata),
                                         REG(a2) APTR userdata
                                         );
ASM void ClosePort(
                   REG(a3) struct MidiDeviceData *data,
                   REG(d0) LONG portnum
                   );

/*   End prototypes  */

/* Name must match the DEVS:Midi/ filename; camd validates it at scan time. */
const struct MidiDeviceData mididevicedata={
  MDD_Magic,
  "ie",
  "ie V1.0 Intuition Engine live-MIDI out (c) 2026 AROS - The AROS Research OS",
  1,
  0,
  Init,
  Expunge,
  OpenPort,
  ClosePort,
  1, /* NPorts: single output port -> cluster "ie.out.0" */
  1
};

SAVEDS ASM BOOL Init(REG(a6) APTR sysbase){
  SysBase=sysbase;
  return TRUE;
}

void Expunge(void){
  /* All-notes-off panic so no voice keeps sounding after the driver unloads. */
  *(volatile UBYTE *)IE_MIDI_LIVE_CTRL = 0x01;
}

/*
   Single output port, so no per-port UserData[] array: ActivateXmit already
   receives userdata in a2. (The debugdriver template's UserData[portnum-1]
   store is also an out-of-bounds write for CAMD's zero-based portnum.)
*/
ULONG (ASM *TransmitFunc)(REG(a2) APTR userdata);

static void ie_emit_port(unsigned char b, void *eud){
  (void)eud;
  *(volatile UBYTE *)IE_MIDI_LIVE_DATA = b;
}

/* AROS-side adapter: preserves the CAMD callback type, keeps ie_xmit.h plain C.
   No function-pointer cast — avoids hiding any calling-convention mismatch. */
static unsigned long ie_next_byte(void *ud){
  return (unsigned long)TransmitFunc((APTR)ud);
}

/*
   Drain the CAMD transmit buffer straight into the IE live-MIDI data port.
   Bytes are forwarded verbatim — no reset between messages — because the IE
   parser relies on MIDI running status.
*/
SAVEDS ASM void ActivateXmit(REG(a2) APTR userdata,ULONG REG(d0) portnum){
  (void)portnum;
  ie_pump_midi(ie_next_byte, userdata, ie_emit_port, NULL);
}

struct MidiPortData midiportdata={
  ActivateXmit
};

SAVEDS ASM struct MidiPortData *OpenPort(
                                         REG(a3) struct MidiDeviceData *data,
                                         REG(d0) LONG portnum,
                                         REG(a0) ULONG (* ASM transmitfunc)(APTR REG(a2) userdata),
                                         REG(a1) void (* ASM receivefunc)(UWORD REG(d0) input,APTR REG(a2) userdata),
                                         REG(a2) APTR userdata
                                         ){
  /* Output-only driver: receivefunc is ignored, matching IE's LiveMIDI. */
  TransmitFunc=transmitfunc;
  return &midiportdata;
}

ASM void ClosePort(
                   REG(a3) struct MidiDeviceData *data,
                   REG(d0) LONG portnum
                   ){
  /* All-notes-off panic so closing the port silences anything still held. */
  *(volatile UBYTE *)IE_MIDI_LIVE_CTRL = 0x01;
}
