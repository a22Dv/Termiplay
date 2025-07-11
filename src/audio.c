#include "tl_pch.h"
#include "tl_errors.h"
#include "tl_types.h"
#include "tl_audio.h"
#include "tl_utils.h"

tl_result apthread_exec(thread_data *data) {
    tl_result excv = TL_SUCCESS;
    CHECK(excv, data == NULL, TL_NULL_ARG, return excv);
    s16_le chunk_buffer = calloc(ABUFFER_BSIZE / sizeof(s16_le), sizeof(s16_le));
    CHECK(excv, chunk_buffer == NULL, TL_ALLOC_FAILURE, return excv);

    ma_device adevice;
   


}

tl_result acthread_exec(thread_data *data) {
    fprintf(stderr, "AC THREAD INIT SUCCESS.\n");
    return TL_SUCCESS;
}