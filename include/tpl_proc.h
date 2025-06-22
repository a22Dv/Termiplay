#ifndef TERMIPLAY_PROC
#define TERMIPLAY_PROC
#define DEBUG

#include <time.h>
#include "tpl_errors.h"
#include "tpl_player.h"

static tpl_result tpl_execute_proc_thread(tpl_thread_data* thread_data) {
    const wpath* vfile_path      = thread_data->p_conf->video_fpath;
    double       thread_clock_ms = 0.0;

    while (true) {
        if (InterlockedOr(thread_data->shutdown_ptr, 0)) {
            return TPL_SUCCESS;
        }
        

        Sleep(1);
        thread_clock_ms += 1;
    }
}
#endif