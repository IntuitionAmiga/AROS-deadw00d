#ifndef AHI_Drivers_IEAudio_DriverData_h
#define AHI_Drivers_IEAudio_DriverData_h

#include <exec/libraries.h>
#include <exec/types.h>
#include <dos/dos.h>

/*
 * DriverBase — matches the Common/DriverBase.h layout that AHI expects.
 * We inline it here to avoid a dependency on the AHI build tree's Common/.
 */
struct DriverBase {
    struct Library  library;
    UWORD           pad;
    BPTR            seglist;
    struct ExecBase *execbase;
};

#define SysBase (AHIsubBase->execbase)

#define DRIVERBASE_SIZEOF (sizeof(struct IEAudioBase))

struct IEAudioBase {
    struct DriverBase driverbase;
};

struct IEAudioData {
    UBYTE           flags;
    UBYTE           pad1;
    BYTE            mastersignal;
    BYTE            slavesignal;
    struct Process *mastertask;
    struct Process *slavetask;
    struct IEAudioBase *ahisubbase;
    APTR            mixbuffer;
};

#endif /* AHI_Drivers_IEAudio_DriverData_h */
