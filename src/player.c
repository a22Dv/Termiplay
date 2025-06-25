#include "tpl_audio.h"
#include "tpl_player.h"
#include "tpl_proc.h"
#include "tpl_types.h"
#include "tpl_video.h"
#include "tpl_ffmpeg_utils.h"
#include "tpl_errors.h"
#include "yaml.h"


#define DEBUG


tpl_result tpl_player_init(
    wpath*            video_fpath,
    wpath*            config_path,
    tpl_player_conf** player_conf

) {
    if (video_fpath == NULL) {
        LOG_ERR(TPL_RECEIVED_NULL);
        return TPL_RECEIVED_NULL;
    }
    if (player_conf == NULL) {
        LOG_ERR(TPL_RECEIVED_NULL);
        return TPL_RECEIVED_NULL;
    }
    if (*player_conf != NULL) {
        LOG_ERR(TPL_OVERWRITE);
        return TPL_OVERWRITE;
    }
    tpl_player_conf* pconf = malloc(sizeof(tpl_player_conf));
    if (pconf == NULL) {
        LOG_ERR(TPL_ALLOC_FAILED);
        return TPL_ALLOC_FAILED;
    }
    pconf->frame_skip      = false;
    pconf->frame_rate      = 0;
    pconf->char_preset1    = NULL;
    pconf->char_preset2    = '\0';
    pconf->gray_scale      = false;
    pconf->preserve_aspect = false;
    pconf->rgb_out         = false;
    pconf->video_fpath     = video_fpath;
    pconf->config_utf8path = NULL;
    pconf->video_duration = 0.0;

    tpl_result dur_call = tpl_av_get_duration(video_fpath, &pconf->video_duration);
    IF_ERR_RET(tpl_failed(dur_call), dur_call);
    tpl_result conv_call = wstr_to_utf8(config_path, &(pconf->config_utf8path));
    if (tpl_failed(conv_call)) {
        LOG_ERR(conv_call);
        return conv_call;
    }

    InitializeSRWLock(&pconf->srw_lock);

    *player_conf = pconf;
    return TPL_SUCCESS;
}

tpl_result tpl_player_setconf(
    tpl_player_conf* pl_config,
    volatile LONG*   write_pending_flag
) {
    if (pl_config == NULL) {
        LOG_ERR(TPL_RECEIVED_NULL);
        return TPL_RECEIVED_NULL;
    }
    if (write_pending_flag == NULL) {
        LOG_ERR(TPL_RECEIVED_NULL);
        return TPL_RECEIVED_NULL;
    }

    yaml_parser_t   yaml_parser;
    FILE*           file = NULL;
    yaml_document_t yaml_doc;
    int             yaml_ret           = 0;
    tpl_result      return_code        = TPL_SUCCESS;
    bool            parser_initialized = false;
    bool            yaml_doc_created   = false;

    bool    rgb_out         = false;
    bool    grayscale       = false;
    bool    frame_skip      = false;
    bool    preserve_aspect = false;
    string* char_preset1    = NULL;
    char    char_preset2    = '\0';
    uint8_t frame_rate      = 0;

    _InterlockedExchange(write_pending_flag, true);
    AcquireSRWLockExclusive(&pl_config->srw_lock);
    file = fopen(str_c_const(pl_config->config_utf8path), "rb");
    if (file == NULL) {
        LOG_ERR(TPL_FAILED_TO_OPEN_CONFIG);
        return_code = TPL_FAILED_TO_OPEN_CONFIG;
        goto cleanup;
    }

    yaml_ret = yaml_parser_initialize(&yaml_parser);
    if (yaml_ret != 1) {
        LOG_ERR(TPL_YAML_ERROR);
        return_code = TPL_YAML_ERROR;
        goto cleanup;
    }
    parser_initialized = true;
    yaml_parser_set_input_file(&yaml_parser, file);
    yaml_ret = yaml_parser_load(&yaml_parser, &yaml_doc);
    if (yaml_ret != 1) {
        LOG_ERR(TPL_YAML_ERROR);
        return_code = TPL_YAML_ERROR;
        goto cleanup;
    }
    yaml_doc_created  = true;
    yaml_node_t* root = yaml_document_get_root_node(&yaml_doc);
    if (root == NULL) {
        LOG_ERR(TPL_INVALID_CONFIG);
        return_code = TPL_INVALID_CONFIG;
        goto cleanup;
    }
    if (root->type != YAML_MAPPING_NODE) {
        LOG_ERR(TPL_INVALID_CONFIG);
        return_code = TPL_INVALID_CONFIG;
        goto cleanup;
    }

    uint8_t access_flags = 0b00000000;

    const yaml_node_pair_t* end_ptr = root->data.mapping.pairs.top;

    for (yaml_node_pair_t* p = root->data.mapping.pairs.start; p < end_ptr; ++p) {
        yaml_node_t* key = yaml_document_get_node(&yaml_doc, p->key);
        yaml_node_t* val = yaml_document_get_node(&yaml_doc, p->value);

        if (key == NULL) {
            LOG_ERR(TPL_INVALID_CONFIG);
            return_code = TPL_INVALID_CONFIG;
            goto cleanup;
        }
        if (val == NULL) {
            LOG_ERR(TPL_INVALID_CONFIG);
            return_code = TPL_INVALID_CONFIG;
            goto cleanup;
        }
        if (key->type != YAML_SCALAR_NODE) {
            LOG_ERR(TPL_INVALID_CONFIG);
            return_code = TPL_INVALID_CONFIG;
            goto cleanup;
        }
        if (val->type != YAML_SCALAR_NODE) {
            LOG_ERR(TPL_INVALID_CONFIG);
            return_code = TPL_INVALID_CONFIG;
            goto cleanup;
        }

        const char* key_utf8 = (char*)key->data.scalar.value;
        const char* val_utf8 = (char*)val->data.scalar.value;

        // Long-ass if-else chain. Refactor given the time with a table approach.
        if (strcmp(key_utf8, "RGB_OUT") == 0) {
            if (access_flags & 0b00000001) {
                LOG_ERR(TPL_INVALID_CONFIG);
                return_code = TPL_INVALID_CONFIG;
                goto cleanup;
            }
            rgb_out = strcmp(val_utf8, "true") == 0;
            access_flags |= 0b00000001;
        } else if (strcmp(key_utf8, "GRAYSCALE") == 0) {
            if (access_flags & 0b00000010) {
                LOG_ERR(TPL_INVALID_CONFIG);
                return_code = TPL_INVALID_CONFIG;
                goto cleanup;
            }
            grayscale = strcmp(val_utf8, "true") == 0;
            access_flags |= 0b00000010;

        } else if (strcmp(key_utf8, "CHAR_PRESET1") == 0) {
            if (access_flags & 0b00000100) {
                LOG_ERR(TPL_INVALID_CONFIG);
                return_code = TPL_INVALID_CONFIG;
                goto cleanup;
            }
            tpl_result init_call = str_init(&char_preset1);
            if (tpl_failed(init_call)) {
                LOG_ERR(init_call);
                return_code = init_call;
                goto cleanup;
            }
            tpl_result mpush_call = str_mulpush(char_preset1, val_utf8);
            if (tpl_failed(mpush_call)) {
                LOG_ERR(mpush_call);
                return_code = mpush_call;
                goto cleanup;
            }
            if (char_preset1->buffer->len == 0) {
                tpl_result fallback_call = str_mulpush(char_preset1, "■");
                if (tpl_failed(fallback_call)) {
                    LOG_ERR(fallback_call);
                    return_code = fallback_call;
                    goto cleanup;
                }
            }
            access_flags |= 0b00000100;

        } else if (strcmp(key_utf8, "FRAME_SKIP") == 0) {
            if (access_flags & 0b00001000) {
                LOG_ERR(TPL_INVALID_CONFIG);
                return_code = TPL_INVALID_CONFIG;
                goto cleanup;
            }
            frame_skip = strcmp(val_utf8, "true") == 0;
            access_flags |= 0b00001000;
        } else if (strcmp(key_utf8, "PRESERVE_ASPECT_RATIO") == 0) {
            if (access_flags & 0b00010000) {
                LOG_ERR(TPL_INVALID_CONFIG);
                return_code = TPL_INVALID_CONFIG;
                goto cleanup;
            }
            preserve_aspect = strcmp(val_utf8, "true") == 0;
            access_flags |= 0b00010000;
        } else if (strcmp(key_utf8, "CHAR_PRESET2") == 0) {
            if (access_flags & 0b00100000) {
                LOG_ERR(TPL_INVALID_CONFIG);
                return_code = TPL_INVALID_CONFIG;
                goto cleanup;
            }
            char_preset2 = val_utf8[0];

            // Fallback.
            if (char_preset2 == '\0') {
                char_preset2 = '■';
            }
            access_flags |= 0b00100000;

        } else if (strcmp(key_utf8, "FRAMERATE") == 0) {
            if (access_flags & 0b01000000) {
                LOG_ERR(TPL_INVALID_CONFIG);
                return_code = TPL_INVALID_CONFIG;
                goto cleanup;
            }

            // NOTE: Silent failure possible when
            // overflow aligns within check boundaries. (300 % 255 = 45)
            // as well as hide typos.
            frame_rate = atoi(val_utf8);
            if (24 > frame_rate || frame_rate > 60) {
                LOG_ERR(TPL_INVALID_CONFIG);
                return_code = TPL_INVALID_CONFIG;
                goto cleanup;
            }
            access_flags |= 0b01000000;
        }

        // Other unknown keys implicitly ignored.
    }
    if (access_flags != 0b01111111) {
        LOG_ERR(TPL_INVALID_CONFIG);
        return_code = TPL_INVALID_CONFIG;
        goto cleanup;
    }

    str_destroy(&pl_config->char_preset1);
    pl_config->char_preset1    = char_preset1;
    pl_config->char_preset2    = char_preset2;
    pl_config->frame_rate      = frame_rate;
    pl_config->frame_skip      = frame_skip;
    pl_config->gray_scale      = grayscale;
    pl_config->preserve_aspect = preserve_aspect;
    pl_config->rgb_out         = rgb_out;
    char_preset1               = NULL; // Moved pointer.

cleanup:
    str_destroy(&char_preset1);
    if (yaml_doc_created) {
        yaml_document_delete(&yaml_doc);
    }
    if (parser_initialized) {
        yaml_parser_delete(&yaml_parser);
    }
    if (file != NULL) {
        fclose(file);
    }
    ReleaseSRWLockExclusive(&pl_config->srw_lock);
    _InterlockedExchange(write_pending_flag, false);
    return return_code;
}

tpl_result tpl_player_setstate(tpl_player_state** pl_state) {
    if (pl_state == NULL) {
        LOG_ERR(TPL_RECEIVED_NULL);
        return TPL_RECEIVED_NULL;
    }
    if (*pl_state != NULL) {
        LOG_ERR(TPL_OVERWRITE);
        return TPL_OVERWRITE;
    }
    tpl_player_state* temp_pl_state = malloc(sizeof(tpl_player_state));
    if (temp_pl_state == NULL) {
        LOG_ERR(TPL_ALLOC_FAILED);
        return TPL_ALLOC_FAILED;
    }
    temp_pl_state->looping           = false;
    temp_pl_state->main_clock        = 0.0;
    temp_pl_state->muted             = false;
    temp_pl_state->playing           = true;
    temp_pl_state->preset_idx        = 0;
    temp_pl_state->seek_multiple_idx = 10.0;
    temp_pl_state->seeking           = false;
    temp_pl_state->vol_lvl           = 0.5;
    InitializeSRWLock(&temp_pl_state->srw_lock);

    *pl_state = temp_pl_state;
    return TPL_SUCCESS;
}

unsigned __stdcall tpl_start_thread(void* data) {
    tpl_thread_data* thread_data = (tpl_thread_data*)data;
    tpl_result       return_code = TPL_SUCCESS;
    switch (thread_data->thread_id) {
    case TPL_AUDIO_THREAD:
        return_code = tpl_execute_audio_thread(thread_data);
        break;
    case TPL_VIDEO_THREAD:
        return_code = tpl_execute_video_thread(thread_data);
        break;
    case TPL_PROC_THREAD:
        return_code = tpl_execute_proc_thread(thread_data);
        break;
    default:
        return_code = TPL_INVALID_ARGUMENT;
        break;
    }
    return return_code;
}

tpl_result tpl_thread_data_create(
    tpl_thread_data** thread_data_ptr,
    uint8_t           thread_id,
    tpl_player_conf*  conf,
    tpl_player_state* state,
    volatile LONG*    pconf_wflag_ptr,
    volatile LONG*    shutdown_ptr,
    volatile LONG*    pstate_wflag_ptr
) {
    if (thread_data_ptr == NULL) {
        LOG_ERR(TPL_RECEIVED_NULL);
        return TPL_RECEIVED_NULL;
    }
    if (pconf_wflag_ptr == NULL) {
        LOG_ERR(TPL_RECEIVED_NULL);
        return TPL_RECEIVED_NULL;
    }
    if (shutdown_ptr == NULL) {
        LOG_ERR(TPL_RECEIVED_NULL);
        return TPL_RECEIVED_NULL;
    }
    if (pstate_wflag_ptr == NULL) {
        LOG_ERR(TPL_RECEIVED_NULL);
        return TPL_RECEIVED_NULL;
    }
    if (conf == NULL) {
        LOG_ERR(TPL_RECEIVED_NULL);
        return TPL_RECEIVED_NULL;
    }
    if (state == NULL) {
        LOG_ERR(TPL_RECEIVED_NULL);
        return TPL_RECEIVED_NULL;
    }
    if (*thread_data_ptr != NULL) {
        LOG_ERR(TPL_OVERWRITE);
        return TPL_OVERWRITE;
    }
    if (1 > thread_id || thread_id > 3) {
        LOG_ERR(TPL_INVALID_ARGUMENT);
        return TPL_INVALID_ARGUMENT;
    }
    tpl_thread_data* th_data = malloc(sizeof(tpl_thread_data));
    if (th_data == NULL) {
        LOG_ERR(TPL_ALLOC_FAILED);
        return TPL_ALLOC_FAILED;
    }
    th_data->p_conf = conf;
    th_data->p_state = state;
    th_data->writer_pconf_flag_ptr = pconf_wflag_ptr;
    th_data->writer_pstate_flag_ptr = pstate_wflag_ptr;
    th_data->shutdown_ptr = shutdown_ptr;
    th_data->thread_id = thread_id;

    *thread_data_ptr = th_data;
    return TPL_SUCCESS;
}