#include "lz4.h"
#include "tl_errors.h"
#include "tl_pch.h"
#include "tl_types.h"
#include "tl_utils.h"
#include "tl_video.h"

/// NOTE: This implementation (malloc in a loop ring buffer) works fine,
/// but for future reference it would be better if you forgo
/// frame compression, as the buffer is extremely tight now,
/// for stability, and turn to a frame pool instead in that ring buffer.

static tl_result get_new_ffmpeg_instance(
    const double      clock_start,
    const WCHAR      *media_path,
    const con_bounds *bounds,
    FILE            **ffmpeg_instance_out
);

static tl_result get_raw_frame(
    FILE             *ffmpeg_stream,
    const con_bounds *bounds,
    raw_frame       **f_out
);

static tl_result get_console_bounds(
    const media_mtdta *mtdta,
    con_bounds       **out
);

static tl_result get_con_frame(
    const con_bounds *bounds,
    const double      ftime,
    const size_t      fnum,
    const dither_mode dmode,
    void            **ext_data,
    wchar_t          *wrk_buffer,
    char             *comp_wbuffer,
    raw_frame        *raw,
    con_frame       **c_out
);

static wchar_t map_to_braille(uint8_t *map);

static tl_result apply_dither(
    void            **ext_data,
    const dither_mode dmode,
    raw_frame        *rframe
);

static tl_result clear_screen(HANDLE stdouth);

tl_result vpthread_exec(thread_data *data) {
    tl_result excv = TL_SUCCESS;
    CHECK(excv, data == NULL, TL_NULL_ARG, return excv);
    player             *pl = data->player;
    FILE               *ffmpeg_stream = NULL;
    size_t              set_serial = 0;
    double              prod_vclock = 0.0;
    double              frametime_start = 0.0;
    size_t              frame_number = 0;
    static const size_t fbuffer_count = VBUFFER_BSIZE / sizeof(con_frame *);

    raw_frame   *staging_frame = malloc(sizeof(raw_frame));
    con_bounds  *bounds = malloc(sizeof(con_bounds));
    wchar_t     *wrk_buffer = malloc(MAXIMUM_BUFFER_SIZE);
    char        *compress_wbuffer = malloc(MAXIMUM_BUFFER_SIZE);
    const WCHAR *media_path = data->player->media_mtdta->media_path;

    CHECK(excv, staging_frame == NULL, TL_ALLOC_FAILURE, return excv);
    CHECK(excv, bounds == NULL, TL_ALLOC_FAILURE, return excv);
    CHECK(excv, wrk_buffer == NULL, TL_ALLOC_FAILURE, return excv);
    CHECK(excv, compress_wbuffer == NULL, TL_ALLOC_FAILURE, return excv);

    staging_frame->data = malloc(MAXIMUM_BUFFER_SIZE);
    CHECK(excv, staging_frame->data == NULL, TL_ALLOC_FAILURE, return excv);

    staging_frame->flength = 0;
    staging_frame->fwidth = 0;

    con_frame *frame = NULL;
    while (true) {
        if (get_atomic_bool(&pl->shutdown)) {
            break;
        }
        if (get_atomic_size_t(&pl->serial) != set_serial) {
            if (ffmpeg_stream) {
                _pclose(ffmpeg_stream);
                ffmpeg_stream = NULL;
            }
            set_atomic_size_t(&pl->vread_idx, 0);
            set_atomic_size_t(&pl->vwrite_idx, 0);
            for (size_t i = 0; i < fbuffer_count; ++i) {
                destroy_conframe(&pl->video_rbuffer[i]);
            }
            set_serial = get_atomic_size_t(&pl->serial);
            while (get_atomic_bool(&pl->invalidated)) {
                if (get_atomic_size_t(&pl->serial) != set_serial ||
                    get_atomic_bool(&pl->shutdown)) {
                    break;
                }
                Sleep(5);
            }
            if (get_atomic_size_t(&pl->serial) != set_serial) {
                continue;
            }
            prod_vclock = get_atomic_double(&pl->main_clock);
            frametime_start = prod_vclock;
            frame_number = 0;
        }
        TRY(excv, get_console_bounds(data->player->media_mtdta, &bounds), goto epilogue);
        TRY(excv, get_new_ffmpeg_instance(prod_vclock, media_path, bounds, &ffmpeg_stream),
            goto epilogue);

        while (true) {
            if (feof(ffmpeg_stream) || get_atomic_bool(&pl->shutdown) ||
                get_atomic_size_t(&pl->serial) != set_serial) {
                break;
            }
            const size_t nwrite_idx = (get_atomic_size_t(&pl->vwrite_idx) + 1) % fbuffer_count;
            while (nwrite_idx == get_atomic_size_t(&pl->vread_idx)) {
                if (get_atomic_bool(&pl->shutdown) ||
                    get_atomic_size_t(&pl->serial) != set_serial) {
                    break;
                }
                Sleep(5);
            }
            if (get_atomic_size_t(&pl->serial) != set_serial || get_atomic_bool(&pl->shutdown)) {
                break;
            }
            TRY(excv, get_raw_frame(ffmpeg_stream, bounds, &staging_frame), goto epilogue);
            if (staging_frame->flength == 0 && staging_frame->fwidth == 0) {
                break;
            }
            frame = NULL;
            TRY(excv,
                get_con_frame(
                    bounds, frametime_start, frame_number, get_atomic_size_t(&pl->dither_mode),
                    (void **)&(pl->ext_assets_ptr), wrk_buffer, compress_wbuffer, staging_frame,
                    &frame
                ),
                goto epilogue);
            frame_number++;
            _InterlockedExchangePointer(
                &pl->video_rbuffer[get_atomic_size_t(&pl->vwrite_idx)], frame
            );
            frame = NULL;
            set_atomic_size_t(&pl->vwrite_idx, nwrite_idx);
        }
        if (feof(ffmpeg_stream)) {
            while (!get_atomic_bool(&pl->shutdown) &&
                   get_atomic_size_t(&pl->serial) == set_serial) {
                Sleep(1);
            }
        }
        _pclose(ffmpeg_stream);
        ffmpeg_stream = NULL;
    }
epilogue:
    // destroy_player() takes care of final free-ing after all threads have been shut down to
    // prevent use-after-free.
    if (ffmpeg_stream) {
        _pclose(ffmpeg_stream);
        ffmpeg_stream = NULL;
    }
    if (frame) {
        destroy_conframe(&frame);
    }
    destroy_rawframe(&staging_frame);
    free(bounds);
    free(wrk_buffer);
    free(compress_wbuffer);
    return excv;
}

static tl_result get_new_ffmpeg_instance(
    const double      clock_start,
    const WCHAR      *media_path,
    const con_bounds *bounds,
    FILE            **ffmpeg_instance_out
) {
    tl_result excv = TL_SUCCESS;
    CHECK(excv, clock_start < 0.0, TL_INVALID_ARG, return excv);
    CHECK(excv, media_path == NULL, TL_NULL_ARG, return excv);
    CHECK(excv, bounds == NULL, TL_NULL_ARG, return excv);
    CHECK(excv, ffmpeg_instance_out == NULL, TL_NULL_ARG, return excv);
    CHECK(excv, *ffmpeg_instance_out != NULL, TL_ALREADY_INITIALIZED, return excv);
    wchar_t cmd[GBUFFER_BSIZE];

    int swret = swprintf_s(
        cmd, GBUFFER_BSIZE,
        L"ffmpeg -v quiet -ss %lf -i \"%ls\" -an -s %zux%zu -f rawvideo -r %u -pix_fmt gray -",
        clock_start, media_path, bounds->log_wdth, bounds->log_ln, V_FPS
    );
    CHECK(excv, swret < 0, TL_FORMAT_FAILURE, return excv);

    *ffmpeg_instance_out = _wpopen(cmd, L"rb");
    CHECK(excv, *ffmpeg_instance_out == NULL, TL_PIPE_CREATION_FAILURE, return excv);
    return excv;
}

static tl_result get_raw_frame(
    FILE             *ffmpeg_stream,
    const con_bounds *bounds,
    raw_frame       **f_out
) {
    tl_result excv = TL_SUCCESS;
    CHECK(excv, ffmpeg_stream == NULL, TL_NULL_ARG, return excv);
    CHECK(excv, bounds == NULL, TL_NULL_ARG, return excv);
    CHECK(excv, f_out == NULL, TL_NULL_ARG, return excv);
    CHECK(excv, *f_out == NULL, TL_NULL_ARG, return excv);
    CHECK(excv, (*f_out)->data == NULL, TL_NULL_ARG, return excv);
    raw_frame *rf = *f_out;
    size_t     px_wdth = bounds->log_wdth;
    size_t     px_ln = bounds->log_ln;
    raw_frame *out = *f_out;

    size_t px_ret = fread(out->data, sizeof(uint8_t), px_wdth * px_ln, ffmpeg_stream);

    // End of video when 0 bytes.
    if (feof(ffmpeg_stream)) {
        rf->flength = 0;
        rf->fwidth = 0;
        return excv;
    }
    CHECK(excv, px_ret != px_wdth * px_ln, TL_INCOMPLETE_DATA, return excv);
    out->flength = px_ln;
    out->fwidth = px_wdth;
    return excv;
}

static tl_result get_con_frame(
    const con_bounds *bounds,
    const double      ftime,
    const size_t      fnum,
    const dither_mode dmode,
    void            **ext_data,
    wchar_t          *wrk_buffer,
    char             *comp_wbuffer,
    raw_frame        *raw,
    con_frame       **c_out
) {
    tl_result excv = TL_SUCCESS;
    CHECK(excv, raw == NULL, TL_NULL_ARG, return excv);
    CHECK(excv, bounds == NULL, TL_NULL_ARG, return excv);
    CHECK(excv, c_out == NULL, TL_NULL_ARG, return excv);
    CHECK(excv, *c_out != NULL, TL_ALREADY_INITIALIZED, return excv);
    if (bounds->abs_conln * bounds->abs_conwdth * sizeof(wchar_t) > MAXIMUM_BUFFER_SIZE ||
        LZ4_compressBound((int)(bounds->cell_ln * bounds->cell_wdth)) > MAXIMUM_BUFFER_SIZE) {
        // Unsupported resolution. `vcthread` will process NULL `con_frame*`s as empty frames.
        return excv;
    }
    *c_out = malloc(sizeof(con_frame));
    CHECK(excv, *c_out == NULL, TL_ALLOC_FAILURE, return excv);
    con_frame *cframe = *c_out;
    cframe->flength = bounds->cell_ln;
    cframe->fwidth = bounds->cell_wdth;
    cframe->uncompressed_bsize = cframe->flength * cframe->fwidth * sizeof(wchar_t);
    cframe->x_start = bounds->start_col;
    cframe->y_start = bounds->start_row;

    TRY(excv, apply_dither(ext_data, dmode, raw), goto epilogue);

    const size_t total_chars = cframe->flength * cframe->fwidth;
    const size_t alignment_matrix[BRAILLE_DOTS_PER_CHAR] = {
        0,
        1,
        bounds->log_wdth,
        bounds->log_wdth + 1,
        bounds->log_wdth * 2,
        bounds->log_wdth * 2 + 1,
        bounds->log_wdth * 3,
        bounds->log_wdth * 3 + 1,
    };

    const size_t total_px = bounds->log_ln * bounds->log_wdth;
    size_t       starting_px_idx = 0;
    uint8_t      bitmap[BRAILLE_DOTS_PER_CHAR] = {0, 0, 0, 0, 0, 0, 0, 0};
    size_t       data_idx = 0;
    size_t       px_idx = 0;

    for (size_t ych = 0; ych < bounds->cell_ln; ++ych) {
        for (size_t xch = 0; xch < bounds->cell_wdth; ++xch) {
            starting_px_idx =
                ych * BRAILLE_CHAR_DOT_LN * bounds->log_wdth + xch * BRAILLE_CHAR_DOT_WDTH;
            for (size_t bdot_i = 0; bdot_i < BRAILLE_DOTS_PER_CHAR; ++bdot_i) {
                px_idx = starting_px_idx + alignment_matrix[bdot_i];
                bitmap[bdot_i] = raw->data[px_idx];
            }
            wchar_t braille_char = map_to_braille(bitmap);
            wrk_buffer[data_idx] = braille_char;
            data_idx++;
        }
    }

    int lz4_compressed_size = LZ4_compress_default(
        (char *)wrk_buffer, comp_wbuffer, (int)cframe->uncompressed_bsize, MAXIMUM_BUFFER_SIZE
    );
    CHECK(excv, lz4_compressed_size == 0, TL_COMPRESS_ERR, goto epilogue);
    cframe->compressed_bsize = (size_t)lz4_compressed_size;
    cframe->compressed_data = malloc(cframe->compressed_bsize);
    cframe->pts = ftime + (fnum * (1 / (double)V_FPS));
    memcpy(cframe->compressed_data, comp_wbuffer, cframe->compressed_bsize);

epilogue:
    if (excv != TL_SUCCESS) {
        destroy_conframe(&cframe);
    }
    return excv;
}

static tl_result get_console_bounds(
    const media_mtdta *mtdta,
    con_bounds       **out
) {
    tl_result excv = TL_SUCCESS;
    CHECK(excv, out == NULL, TL_NULL_ARG, return excv);
    CHECK(excv, *out == NULL, TL_INVALID_ARG, return excv);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    HANDLE                     stdouth = GetStdHandle(STD_OUTPUT_HANDLE);
    CHECK(excv, stdouth == NULL || stdouth == INVALID_HANDLE_VALUE, TL_OS_ERR, return excv);
    CHECK(excv, !GetConsoleScreenBufferInfo(stdouth, &csbi), TL_CONSOLE_ERR, return excv);

    con_bounds *b = *out;

    // We make this shorter by 1 so that we have space for playback info at the top.
    b->cell_ln = csbi.srWindow.Bottom - csbi.srWindow.Top;
    b->cell_wdth = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    b->abs_conln = b->cell_ln;
    b->abs_conwdth = b->cell_wdth;

    const double char_pixel_aspect = (double)BRAILLE_CHAR_DOT_WDTH / (double)BRAILLE_CHAR_DOT_LN;
    const double con_pixel_aspect = ((double)b->cell_wdth / (double)b->cell_ln) * char_pixel_aspect;
    const double v_aspect = (double)mtdta->width / (double)mtdta->height;

    if (v_aspect > con_pixel_aspect) {
        // Video wider than console.
        b->cell_ln = (size_t)(((double)b->cell_wdth * char_pixel_aspect) / v_aspect);
    } else {
        // Video narrower than console.
        b->cell_wdth = (size_t)(((double)b->cell_ln / char_pixel_aspect) * v_aspect);
    }

    // Ensure the pixel dimensions are an even multiple of the braille character dots.
    if (b->log_wdth % BRAILLE_CHAR_DOT_WDTH != 0) {
        b->log_wdth -= (b->log_wdth % BRAILLE_CHAR_DOT_WDTH);
    }
    if (b->log_ln % BRAILLE_CHAR_DOT_LN != 0) {
        b->log_ln -= (b->log_ln % BRAILLE_CHAR_DOT_LN);
    }
    b->log_ln = b->cell_ln * BRAILLE_CHAR_DOT_LN;
    b->log_wdth = b->cell_wdth * BRAILLE_CHAR_DOT_WDTH;

    // Center the output frame in the console window
    b->start_row = (b->abs_conln - b->cell_ln) / 2;
    b->start_col = (b->abs_conwdth - b->cell_wdth) / 2;
    return excv;
}

static wchar_t map_to_braille(uint8_t *map) {
    static const wchar_t main_offset = 0x2800;
    static const uint8_t alignment_shifts[BRAILLE_DOTS_PER_CHAR] = {0, 3, 1, 4, 2, 5, 6, 7};
    wchar_t              bchar = main_offset;
    uint8_t              set = 0;
    for (size_t i = 0; i < BRAILLE_DOTS_PER_CHAR; ++i) {
        set = map[i] < 128 ? 0 : 1;
        bchar |= (set << alignment_shifts[i]);
    }
    return bchar;
}

tl_result vcthread_exec(thread_data *data) {
    static const size_t vbuffer_frames = VBUFFER_BSIZE / sizeof(con_frame *);
    static const COORD  hm = {.X = 0, .Y = 1};

    tl_result   excv = TL_SUCCESS;
    player     *pl = data->player;
    size_t      set_serial = 0;
    bool        debug_print = false;
    con_frame **fbuf = pl->video_rbuffer;

    CHAR_INFO *conbuf = NULL;
    SMALL_RECT write_region = {.Bottom = 0, .Left = 0, .Right = 0, .Top = 0};
    COORD      conbuf_size = {.X = 0, .Y = 0};
    HANDLE     stdouth = GetStdHandle(STD_OUTPUT_HANDLE);
    CHECK(excv, stdouth == NULL || stdouth == INVALID_HANDLE_VALUE, TL_OS_ERR, return excv);

    wchar_t *uncomp_fbuffer = malloc(MAXIMUM_BUFFER_SIZE);
    CHECK(excv, uncomp_fbuffer == NULL, TL_ALLOC_FAILURE, return excv);

    while (true) {
        const bool   shutdown = get_atomic_bool(&pl->shutdown);
        const bool   playback = get_atomic_bool(&pl->playing);
        const size_t cserial = get_atomic_size_t(&pl->serial);
        const bool   ndebug_print = get_atomic_bool(&pl->debug_print);
        if (shutdown) {
            break;
        }

        // Just quickly clears left behind status prints.
        if (!ndebug_print && debug_print) {
            TRY(excv, clear_screen(stdouth), goto epilogue);
        }
        debug_print = ndebug_print;

        if (cserial != set_serial) {
            set_serial = cserial;

            // Prevents resetting again and again while seeking.
            while (get_atomic_bool(&pl->invalidated)) {
                if (shutdown || set_serial != cserial) {
                    break;
                }
                Sleep(5);
            }
            if (get_atomic_size_t(&pl->serial) != set_serial) {
                continue;
            }
            // Removes left-behind artifacts upon resizing.
            TRY(excv, clear_screen(stdouth), goto epilogue);
        }
        if (!playback) {
            Sleep(10);
            continue;
        }

        const size_t vread = get_atomic_size_t(&pl->vread_idx);
        const size_t vwrite = get_atomic_size_t(&pl->vwrite_idx);
        const size_t nvread = (vread + 1) % vbuffer_frames;

        if (vread == vwrite) {
            Sleep(5);
            continue;
        }
        if (fbuf[vread] == NULL) {
            clear_screen(stdouth);
            set_atomic_size_t(&pl->vread_idx, nvread);
            Sleep(5);
            continue;
        }

        // We need to take ownership of frame
        const con_frame *frame = _InterlockedExchangePointer((PVOID volatile *)&fbuf[vread], NULL);
        const size_t     tchars = frame->flength * frame->fwidth;
        
        if (conbuf == NULL || frame->flength != conbuf_size.Y || frame->fwidth != conbuf_size.X) {
            free(conbuf);
            conbuf = malloc(frame->flength * frame->fwidth * sizeof(CHAR_INFO));
            CHECK(excv, conbuf == NULL, TL_ALLOC_FAILURE, goto epilogue);
            conbuf_size.X = (SHORT)frame->fwidth;
            conbuf_size.Y = (SHORT)frame->flength;
            write_region.Left = (SHORT)frame->x_start;

            // + 1 to make sure the frame doesn't overlap with the stat print.
            write_region.Top = (SHORT)frame->y_start + 1;
            write_region.Right = (SHORT)(frame->x_start + frame->fwidth - 1);
            write_region.Bottom = (SHORT)(frame->y_start + frame->flength - 1);
        }
        const double pts = frame->pts;
        AcquireSRWLockShared(&pl->srw_mclock);
        const double clock = get_atomic_double(&pl->main_clock);
        ReleaseSRWLockShared(&pl->srw_mclock);
        const double drift = clock - pts;

        if (drift > 1.0) {
            destroy_conframe(&frame);
            set_atomic_size_t(&pl->vread_idx, nvread);
            continue;
        }
        if (drift < -(1 / (double)V_FPS)) {
            // -drift to turn it positive again.
            Sleep((DWORD)(-drift * 1000.0));
        }
        int lz4_dret = LZ4_decompress_fast(
            frame->compressed_data, (char *)uncomp_fbuffer, (int)frame->uncompressed_bsize
        );
        CHECK(excv, lz4_dret == 0, TL_COMPRESS_ERR, goto epilogue);
        for (size_t i = 0; i < tchars; ++i) {
            conbuf[i].Char.UnicodeChar = uncomp_fbuffer[i];
        }
        const DWORD clr_mode = (DWORD)get_atomic_size_t(&pl->color_mode);
        for (size_t i = 0; i < tchars; ++i) {

            // Background color is implicit black/default from attributes set to 0.
            conbuf[i].Attributes = 0;
            conbuf[i].Attributes |= clr_mode;
        }
        CHECK(
            excv, !WriteConsoleOutputW(stdouth, conbuf, conbuf_size, hm, &write_region),
            TL_CONSOLE_ERR, goto epilogue
        );
        destroy_conframe(&frame);
        set_atomic_size_t(&pl->vread_idx, nvread);
    }
epilogue:
    clear_screen(stdouth);
    free(conbuf);
    free(uncomp_fbuffer);
    return excv;
}

static tl_result clear_screen(HANDLE stdouth) {
    tl_result excv = TL_SUCCESS;
    CHECK(excv, stdouth == NULL || stdouth == INVALID_HANDLE_VALUE, TL_INVALID_ARG, return excv);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    CHECK(excv, !GetConsoleScreenBufferInfo(stdouth, &csbi), TL_OS_ERR, return excv);
    const DWORD cellc = csbi.dwSize.X * csbi.dwSize.Y;
    const COORD hm = {.X = 0, .Y = 0};
    DWORD       coutc = 0;
    CHECK(
        excv, !FillConsoleOutputCharacterW(stdouth, L' ', cellc, hm, &coutc), TL_CONSOLE_ERR,
        return excv
    );
    CHECK(
        excv, !FillConsoleOutputAttribute(stdouth, csbi.wAttributes, cellc, hm, &coutc),
        TL_CONSOLE_ERR, return excv
    );
    CHECK(excv, !SetConsoleCursorPosition(stdouth, hm), TL_CONSOLE_ERR, return excv);
    return excv;
}

static tl_result threshold(
    void     **_ext_data,
    raw_frame *rf
) {
    // No-op. Thresholding is handled by the converter instead (<128 & >=128).
    return TL_SUCCESS;
}

static tl_result flyd_stnbrg(
    void     **_ext_data,
    raw_frame *rf
) {
    tl_result excv = TL_SUCCESS;
    CHECK(excv, rf == NULL, TL_NULL_ARG, return excv);
    static const uint8_t kernel[FLOYD_STEINBERG_KERNEL_SIZE] = {
        (uint8_t)(256 * (float)7 / 16), (uint8_t)(256 * (float)3 / 16),
        (uint8_t)(256 * (float)3 / 16), (uint8_t)(256 * (float)3 / 16)
    };
    static size_t idxs[FLOYD_STEINBERG_KERNEL_SIZE] = {0, 0, 0, 0};
    static bool   is_valid[FLOYD_STEINBERG_KERNEL_SIZE] = {false, false, false, false};
    size_t        cidx = 0;
    uint8_t       value = 0;
    int16_t       delta = 0;
    int16_t       diffuse = 0;

    for (size_t y = 0; y < rf->flength; ++y) {
        for (size_t x = 0; x < rf->fwidth; ++x) {
            cidx = y * rf->fwidth + x;
            value = rf->data[cidx] < 128 ? 0 : 255;
            delta = rf->data[cidx] - value;
            rf->data[cidx] = value;

            idxs[0] = cidx + 1;
            idxs[1] = cidx - 1 + rf->fwidth;
            idxs[2] = cidx + rf->fwidth;
            idxs[3] = cidx + 1 + rf->fwidth;

            is_valid[0] = (x + 1) < rf->fwidth;
            is_valid[1] = x > 0 && (y + 1) < rf->flength;
            is_valid[2] = (y + 1) < rf->flength;
            is_valid[3] = (x + 1) < rf->fwidth && (y + 1) < rf->flength;

            for (size_t i = 0; i < FLOYD_STEINBERG_KERNEL_SIZE; ++i) {
                if (!is_valid[i]) {
                    continue;
                }
                diffuse = rf->data[idxs[i]] + ((delta * kernel[i]) >> 8);
                diffuse = diffuse > 0 ? diffuse : 0;
                diffuse = diffuse < 255 ? diffuse : 255;
                rf->data[idxs[i]] = (uint8_t)diffuse;
            }
        }
    }
    return excv;
}

static tl_result blue_dth(
    void     **ext_data,
    raw_frame *rf
) {
    static const WCHAR *btexture_pth = L"assets\\bnoise.raw";
    static const size_t btexture_length = 8192;
    static const size_t btexture_width = 8192;
    static bool         reload = true;
    static size_t       set_length = 0;
    static size_t       set_width = 0;
    static size_t       fcount = 0;
    static const size_t threshold[DTH_BLUE_MODES] = {V_FPS - 8, V_FPS - 15, V_FPS - 23, 0};
    tl_result           excv = TL_SUCCESS;
    WCHAR               exec_path[MAX_PATH];
    WCHAR               ftexture_pth[MAX_PATH];
    FILE               *data = NULL;
    uint8_t            *texture = NULL;
    CHECK(
        excv,
        rf->flength == 0 || rf->fwidth == 0 || rf->flength > btexture_length ||
            rf->fwidth > btexture_width,
        TL_INVALID_ARG, goto epilogue
    );
    CHECK(excv, rf == NULL, TL_NULL_ARG, return excv);
    CHECK(excv, ext_data == NULL, TL_NULL_ARG, return excv);
    if (set_length != rf->flength || set_width != rf->fwidth) {
        set_length = rf->flength;
        set_width = rf->fwidth;
        reload = true;
    }
    if (reload) {
        DWORD get_exec = GetModuleFileNameW(NULL, exec_path, MAX_PATH);
        CHECK(excv, get_exec >= MAX_PATH || get_exec == 0, TL_OS_ERR, goto epilogue);
        HRESULT hr_remove = PathCchRemoveFileSpec(exec_path, MAX_PATH);
        CHECK(excv, !SUCCEEDED(hr_remove), TL_OS_ERR, goto epilogue);
        HRESULT hr_combine = PathCchCombine(ftexture_pth, MAX_PATH, exec_path, btexture_pth);
        CHECK(excv, !SUCCEEDED(hr_combine), TL_OS_ERR, goto epilogue);
        DWORD fattr = GetFileAttributesW(ftexture_pth);
        CHECK(excv, fattr == INVALID_FILE_ATTRIBUTES, TL_DEP_NOT_FOUND, goto epilogue);

        free(_InterlockedExchangePointer((volatile PVOID *)ext_data, NULL));
        texture = malloc(rf->flength * rf->fwidth * sizeof(uint8_t));
        CHECK(excv, texture == NULL, TL_ALLOC_FAILURE, goto epilogue);
        errno_t data_open = _wfopen_s(&data, ftexture_pth, L"rb");
        CHECK(excv, data == NULL || data_open != 0, TL_PIPE_CREATION_FAILURE, goto epilogue);
        const size_t bskip = (btexture_width - rf->fwidth) * sizeof(uint8_t);
        for (size_t i = 0; i < set_length; ++i) {
            size_t fret = fread(texture + (i * rf->fwidth), sizeof(uint8_t), rf->fwidth, data);
            CHECK(excv, fret != rf->fwidth, TL_PIPE_READ_FAILURE, goto epilogue);
            if (bskip > 0) {
                int fret_discard = fseek(data, (long)bskip, SEEK_CUR);
                CHECK(excv, fret_discard != 0, TL_PIPE_READ_FAILURE, goto epilogue);
            }
        }
        int exret = fclose(data);
        data = NULL;
        CHECK(excv, exret != 0, TL_PIPE_PROC_FAILURE, goto epilogue);
        _InterlockedExchangePointer((volatile PVOID *)ext_data, (PVOID)texture);
        reload = false;
    }
    const size_t mod_fps = fcount % V_FPS;
    uint8_t     *frame_data = rf->data;
    size_t       mode = 0;
    for (size_t i = 0; i < DTH_BLUE_MODES; ++i) {
        if (threshold[i] > mod_fps) {
            mode++;
            continue;
        }
        break;
    }
    const size_t total_px = set_length * set_width;
    const size_t start_idx[DTH_BLUE_MODES] = {
        0, total_px - 1, rf->fwidth - 1, total_px - rf->fwidth
    };
    const size_t access_column[DTH_BLUE_MODES] = {0, 0, 2 * rf->fwidth, -2 * rf->fwidth};
    const int8_t access_width[DTH_BLUE_MODES] = {1, -1, -1, 1};

    size_t fidx = 0;
    size_t txt_idx = start_idx[mode];
    for (size_t y = 0; y < rf->flength; ++y) {
        for (size_t x = 0; x < rf->fwidth; ++x) {
            fidx = y * rf->fwidth + x;
            if (frame_data[fidx] > ((uint8_t *)(*ext_data))[txt_idx]) {
                frame_data[fidx] = UINT8_MAX;
            } else {
                frame_data[fidx] = 0;
            }
            txt_idx += access_width[mode];
        }
        txt_idx += access_column[mode];
    }
    fcount++;
epilogue:
    if (excv != TL_SUCCESS) {
        if (data) {
            fclose(data);
            data = NULL;
        }
        free(texture);
    }
    return excv;
}

static tl_result halftone(
    void     **ext_data,
    raw_frame *rf
) {
    tl_result excv = TL_SUCCESS;
    CHECK(excv, rf == NULL, TL_NULL_ARG, return excv);

    // We use a spiral pattern dither here.
    static const uint8_t matrix[HALFTONE_MATRIX_SIZE] = {
        (uint8_t)(255 * 10 / (double)16), (uint8_t)(255 * 9 / (double)16),
        (uint8_t)(255 * 8 / (double)16),  (uint8_t)(255 * 7 / (double)16),
        (uint8_t)(255 * 11 / (double)16), (uint8_t)(255 * 16 / (double)16),
        (uint8_t)(255 * 15 / (double)16), (uint8_t)(255 * 6 / (double)16),
        (uint8_t)(255 * 12 / (double)16), (uint8_t)(255 * 13 / (double)16),
        (uint8_t)(255 * 14 / (double)16), (uint8_t)(255 * 5 / (double)16),
        (uint8_t)(255 * 1 / (double)16),  (uint8_t)(255 * 2 / (double)16),
        (uint8_t)(255 * 3 / (double)16),  (uint8_t)(255 * 4 / (double)16)
    };
    uint8_t  threshold_idx = 0;
    size_t   data_idx = 0;
    uint8_t *data = rf->data;
    for (size_t y = 0; y < rf->flength; ++y) {
        for (size_t x = 0; x < rf->fwidth; ++x) {
            data_idx = y * rf->fwidth + x;
            threshold_idx = ((y & 3) << 2) | (x & 3);
            data[data_idx] = matrix[threshold_idx] > data[data_idx] ? 0 : 255;
        }
    }
    return TL_SUCCESS;
}

/// @brief A modified version of Sierra Lite.
static tl_result sierra_lite(
    void     **_ext_data,
    raw_frame *rf
) {
    tl_result excv = TL_SUCCESS;
    CHECK(excv, rf == NULL, TL_NULL_ARG, return excv);

    // The original kernel discarded 25% of the error, here we use all of it,
    // split it evenly, and shift the bottom kernel to the bottom left.
    static const uint8_t kernel[SIERRA_LITE_KERNEL_SIZE] = {128, 128};
    static size_t        idxs[SIERRA_LITE_KERNEL_SIZE] = {0, 0};
    static bool          is_valid[SIERRA_LITE_KERNEL_SIZE] = {false, false};
    size_t               cidx = 0;
    uint8_t              value;
    int16_t              delta = 0;
    int16_t              diffuse = 0;

    for (size_t y = 0; y < rf->flength; ++y) {
        for (size_t x = 0; x < rf->fwidth; ++x) {
            cidx = y * rf->fwidth + x;
            value = rf->data[cidx] < 128 ? 0 : 255;
            delta = rf->data[cidx] - value;
            rf->data[cidx] = value;

            idxs[0] = cidx + 1;
            idxs[1] = cidx - 1 + rf->fwidth;
            is_valid[0] = (x + 1) < rf->fwidth;
            is_valid[1] = (x > 0) && (y + 1) < rf->flength;

            for (size_t i = 0; i < SIERRA_LITE_KERNEL_SIZE; ++i) {
                if (!is_valid[i]) {
                    continue;
                }
                diffuse = rf->data[idxs[i]] + ((delta * kernel[i]) >> 8);
                diffuse = diffuse > 0 ? diffuse : 0;
                diffuse = diffuse < 255 ? diffuse : 255;
                rf->data[idxs[i]] = (uint8_t)diffuse;
            }
        }
    }
    return excv;
}

static tl_result bayer_4x4(
    void     **_ext_data,
    raw_frame *rf
) {
    tl_result excv = TL_SUCCESS;
    CHECK(excv, rf == NULL, TL_NULL_ARG, return excv);

    // The mathematical bayer matrix was supposed to have 0 at the first entry (0, 0)
    // but I changed it to have a threshold of 1 for aesthetic purposes allowing for deep blacks.
    static const uint8_t matrix[BAYER_4X4_MATRIX_SIZE] = {15, 127, 31, 159, 191, 63,  223, 95,
                                                          47, 175, 15, 143, 239, 111, 207, 79};

    uint8_t  threshold_idx = 0;
    size_t   data_idx = 0;
    uint8_t *data = rf->data;
    for (size_t y = 0; y < rf->flength; ++y) {
        for (size_t x = 0; x < rf->fwidth; ++x) {
            data_idx = y * rf->fwidth + x;
            threshold_idx = ((y & 3) << 2) | (x & 3);
            data[data_idx] = matrix[threshold_idx] > data[data_idx] ? 0 : 255;
        }
    }
    return TL_SUCCESS;
}

static tl_result bayer_8x8(
    void     **_ext_data,
    raw_frame *rf
) {
    tl_result excv = TL_SUCCESS;
    CHECK(excv, rf == NULL, TL_NULL_ARG, return excv);

    // The mathematical bayer matrix was supposed to have 0
    // but I changed it to have a threshold of 1 for aesthetic purposes allowing for deep blacks.
    static const uint8_t matrix[BAYER_8X8_MATRIX_SIZE] = {
        3,  127, 31, 159, 7,  135, 39, 167, 191, 63,  223, 95,  199, 71,  231, 103,
        47, 175, 15, 143, 55, 183, 23, 151, 239, 111, 207, 79,  247, 119, 215, 87,
        11, 139, 43, 171, 3,  131, 35, 163, 203, 75,  235, 107, 195, 67,  227, 99,
        59, 187, 27, 155, 51, 179, 19, 147, 251, 123, 219, 91,  243, 115, 211, 83
    };

    uint8_t  threshold_idx = 0;
    size_t   data_idx = 0;
    uint8_t *data = rf->data;
    for (size_t y = 0; y < rf->flength; ++y) {
        for (size_t x = 0; x < rf->fwidth; ++x) {
            data_idx = y * rf->fwidth + x;
            threshold_idx = ((y & 7) << 3) | (x & 7);
            data[data_idx] = matrix[threshold_idx] > data[data_idx] ? 0 : 255;
        }
    }
    return TL_SUCCESS;
}

static tl_result bayer_16x16(
    void     **_ext_data,
    raw_frame *rf
) {
    tl_result excv = TL_SUCCESS;
    CHECK(excv, rf == NULL, TL_NULL_ARG, return excv);

    // The mathematical bayer matrix was supposed to have 0
    // but I changed it to have a threshold of 1 for aesthetic purposes allowing for deep blacks.
    static const uint8_t matrix[BAYER_16X16_MATRIX_SIZE] = {
        0,   128, 32,  160, 8,   136, 40,  168, 2,   130, 34,  162, 10,  138, 42,  170, 192, 64,
        224, 96,  200, 72,  232, 104, 194, 66,  226, 98,  202, 74,  234, 106, 48,  176, 16,  144,
        56,  184, 24,  152, 50,  178, 18,  146, 58,  186, 26,  154, 240, 112, 208, 80,  248, 120,
        216, 88,  242, 114, 210, 82,  250, 122, 218, 90,  12,  140, 44,  172, 4,   132, 36,  164,
        14,  142, 46,  174, 6,   134, 38,  166, 204, 76,  236, 108, 196, 68,  228, 100, 206, 78,
        238, 110, 198, 70,  230, 102, 60,  188, 28,  156, 52,  180, 20,  148, 62,  190, 30,  158,
        54,  182, 22,  150, 252, 124, 220, 92,  244, 116, 212, 84,  254, 126, 222, 94,  246, 118,
        214, 86,  3,   131, 35,  163, 11,  139, 43,  171, 1,   129, 33,  161, 9,   137, 41,  169,
        195, 67,  227, 99,  203, 75,  235, 107, 197, 69,  229, 101, 205, 77,  237, 109, 51,  179,
        19,  147, 59,  187, 27,  155, 49,  177, 17,  145, 57,  185, 25,  153, 243, 115, 211, 83,
        251, 123, 219, 91,  241, 113, 209, 81,  249, 121, 217, 89,  15,  143, 47,  175, 7,   135,
        39,  167, 13,  141, 45,  173, 5,   133, 37,  165, 207, 79,  239, 111, 199, 71,  231, 103,
        209, 81,  241, 113, 201, 73,  233, 105, 63,  191, 31,  159, 55,  183, 23,  151, 61,  189,
        29,  157, 53,  181, 21,  149, 255, 127, 223, 95,  247, 119, 215, 87,  253, 125, 221, 93,
        245, 117, 213, 85
    };
    uint8_t  threshold_idx = 0;
    size_t   data_idx = 0;
    uint8_t *data = rf->data;
    for (size_t y = 0; y < rf->flength; ++y) {
        for (size_t x = 0; x < rf->fwidth; ++x) {
            data_idx = y * rf->fwidth + x;
            threshold_idx = ((y & 15) << 4) | (x & 15);
            data[data_idx] = matrix[threshold_idx] >= data[data_idx] ? 0 : 255;
        }
    }
    return TL_SUCCESS;
}

static tl_result apply_dither(
    void            **ext_data,
    const dither_mode dmode,
    raw_frame        *rframe
) {
    tl_result excv = TL_SUCCESS;
    CHECK(excv, dmode < 0 || dmode > DTH_MODES, TL_INVALID_ARG, return excv);
    CHECK(excv, rframe == NULL, TL_NULL_ARG, return excv);
    CHECK(excv, ext_data == NULL, TL_NULL_ARG, return excv);
    static bool setup = true;
    static tl_result (*(dither_funcs[DTH_MODES]))(void **ext_data, raw_frame *);
    if (setup) {
        dither_funcs[DTH_THRESHOLDING] = threshold;
        dither_funcs[DTH_FLOYD_STEINBERG] = flyd_stnbrg;
        dither_funcs[DTH_BAYER_4X4] = bayer_4x4;
        dither_funcs[DTH_BAYER_8X8] = bayer_8x8;
        dither_funcs[DTH_BAYER_16X16] = bayer_16x16;
        dither_funcs[DTH_BLUE] = blue_dth;
        dither_funcs[DTH_HALFTONE] = halftone;
        dither_funcs[DTH_SIERRA_LITE] = sierra_lite;
        setup = false;
    }
    TRY(excv, dither_funcs[dmode](ext_data, rframe), return excv);
    return excv;
}
