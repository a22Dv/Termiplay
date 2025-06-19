#include <stdbool.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>  
#include <libavutil/avutil.h> 
#include "tpl_errors.h"
#include "tpl_types.h"
#include "tpl_path.h"

static tpl_result tpl_av_streams_exist(string* utf8_file_path, bool* result) {
    string* fp_utf8 = NULL;
    
    // Format context.
    AVFormatContext *fmtctx = NULL;

    // A/V stream indices.
    int vid_stridx = -1;
    int aud_stridx = -1;
    int return_val = 0;

    // Buffer to store FFmpeg's error messages.
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
    *result = true;
    return TPL_SUCCESS;
}