#include "tl_proc.h"

tl_result write_abuffer(
    const size_t  serial_at_call,
    player_state *pstate,
    const double  clock_start,
    const WCHAR  *media_path,
    int16_t      *buffer
){
    Sleep(350);

    // Optimally called in intervals. Say every N frames.
    // Although for audio immediately after the ffmpeg call.
    AcquireSRWLockShared(&pstate->srw);
    const updated_serial = pstate->current_serial;
    ReleaseSRWLockShared(&pstate->srw);
    if (serial_at_call != updated_serial) {
        return TL_OUTDATED_SERIAL;
    }
    return TL_SUCCESS;
}

tl_result write_vbuffer_compressed(
    const size_t  serial_at_call,
    player_state *pstate,
    const double  clock_start,
    const WCHAR  *media_path,
    char        **buffer,
    const bool    default_charset
) {
    Sleep(350);

    // Optimally called in intervals. Say every N frames.
    AcquireSRWLockShared(&pstate->srw);
    const updated_serial = pstate->current_serial;
    ReleaseSRWLockShared(&pstate->srw);
    if (serial_at_call != updated_serial) {
        return TL_OUTDATED_SERIAL;
    }
    return TL_SUCCESS;
}