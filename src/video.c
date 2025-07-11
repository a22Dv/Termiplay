#include "tl_pch.h"
#include "tl_errors.h"
#include "tl_types.h"
#include "tl_video.h"

tl_result vpthread_exec(thread_data *data) {
    fprintf(stderr, "VP THREAD INIT SUCCESS.\n");
    return TL_SUCCESS;
}

tl_result vcthread_exec(thread_data *data) {
    fprintf(stderr, "VC THREAD INIT SUCCESS.\n");
    return TL_SUCCESS;
}