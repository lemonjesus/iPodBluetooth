/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2005 by Linus Nielsen Feltzing
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/
#ifndef AUDIO_H
#define AUDIO_H

#include <stdbool.h>
#include <string.h> /* size_t */
#include "config.h"
/* These must always be included with audio.h for this to compile under
   cetain conditions. Do it here or else spread the complication around to
   many files. */
#if CONFIG_CODEC == SWCODEC
#include "pcm_sampr.h"
#include "pcm.h"
#ifdef HAVE_RECORDING
#include "enc_base.h"
#endif /* HAVE_RECORDING */
#endif /* CONFIG_CODEC == SWCODEC */

#define AUDIO_STATUS_PLAY       0x0001
#define AUDIO_STATUS_PAUSE      0x0002
#define AUDIO_STATUS_RECORD     0x0004
#define AUDIO_STATUS_PRERECORD  0x0008
#define AUDIO_STATUS_ERROR      0x0010
#define AUDIO_STATUS_WARNING    0x0020

#define AUDIOERR_DISK_FULL      1

#define AUDIO_GAIN_LINEIN       0
#define AUDIO_GAIN_MIC          1


void audio_init(void) INIT_ATTR;
void audio_play(unsigned long elapsed, unsigned long offset);
void audio_stop(void);
/* Stops audio from serving playback and frees resources*/
void audio_hard_stop(void);
void audio_pause(void);
void audio_resume(void);
void audio_next(void);
void audio_prev(void);
int audio_status(void);
/* size of the audio buffer */
size_t audio_buffer_size(void);
/* size of the buffer available for allocating memory from the audio buffer using core_*()
 * returns core_available() if audio buffer is not allocated yet */
size_t audio_buffer_available(void);
void audio_ff_rewind(long newpos);
void audio_flush_and_reload_tracks(void);
struct mp3entry* audio_current_track(void);
struct mp3entry* audio_next_track(void);
bool audio_peek_track(struct mp3entry* id3, int offset);
#ifdef HAVE_DISK_STORAGE
void audio_set_buffer_margin(int setting);
#endif
void audio_error_clear(void);
int audio_get_file_pos(void);
void audio_beep(int duration);

#if CONFIG_CODEC == SWCODEC
void audio_next_dir(void);
void audio_prev_dir(void);

#else /* hwcodec only */
struct audio_debug
{
        int audiobuflen;
        int audiobuf_write;
        int audiobuf_swapwrite;
        int audiobuf_read;

        int last_dma_chunk_size;

        bool dma_on;
        bool playing;
        bool play_pending;
        bool is_playing;
        bool filling;
        bool dma_underrun;

        int unplayed_space;
        int playable_space;
        int unswapped_space;

        int low_watermark_level;
        int lowest_watermark_level;
};

void audio_get_debugdata(struct audio_debug *dbgdata);
/* unsigned int audio_error(void); - unused function */
void audio_init_playback(void);

#define audio_next_dir() ({ })
#define audio_prev_dir() ({ })

#endif

/* channel modes */
enum rec_channel_modes
{
    __CHN_MODE_START_INDEX = -1,

    CHN_MODE_STEREO,
    CHN_MODE_MONO,

    CHN_NUM_MODES
};

#if CONFIG_CODEC == SWCODEC
/* channel mode capability bits */
#define CHN_CAP_STEREO  (1 << CHN_MODE_STEREO)
#define CHN_CAP_MONO    (1 << CHN_MODE_MONO)
#define CHN_CAP_ALL     (CHN_CAP_STEREO | CHN_CAP_MONO)
#endif /* CONFIG_CODEC == SWCODEC */

/* the enums below must match prestr[] in recording.c */
enum audio_sources
{
    AUDIO_SRC_PLAYBACK = -1, /* Virtual source */
    HAVE_MIC_IN_(AUDIO_SRC_MIC,)
    HAVE_LINE_IN_(AUDIO_SRC_LINEIN,)
    HAVE_SPDIF_IN_(AUDIO_SRC_SPDIF,)
    HAVE_FMRADIO_IN_(AUDIO_SRC_FMRADIO,)
    AUDIO_NUM_SOURCES,
    AUDIO_SRC_MAX = AUDIO_NUM_SOURCES-1,
    AUDIO_SRC_DEFAULT = AUDIO_SRC_PLAYBACK
};

extern int audio_channels;
extern int audio_output_source;

#ifdef HAVE_RECORDING
/* Recordable source implies it has the input as well */

/* For now there's no restrictions on any targets with which inputs
   are recordable so define them as equivalent - if they do differ,
   special handling is needed right now. */
enum rec_sources
{
    __REC_SRC_FIRST = -1,
    HAVE_MIC_REC_(REC_SRC_MIC,)
    HAVE_LINE_REC_(REC_SRC_LINEIN,)
    HAVE_SPDIF_REC_(REC_SRC_SPDIF,)
    HAVE_FMRADIO_REC_(REC_SRC_FMRADIO,)
    REC_NUM_SOURCES
};
#endif /* HAVE_RECORDING */

#if CONFIG_CODEC == SWCODEC
/* selects a source to monitor for recording or playback */
#define SRCF_PLAYBACK         0x0000    /* default */
#define SRCF_RECORDING        0x1000
#if CONFIG_TUNER
/* for AUDIO_SRC_FMRADIO */
#define SRCF_FMRADIO_PLAYING  0x0000    /* default */
#define SRCF_FMRADIO_PAUSED   0x2000
#endif
#endif

#ifdef HAVE_RECORDING
/* parameters for audio_set_recording_options */
struct audio_recording_options
{
    int  rec_source;
    int  rec_frequency;
    int  rec_channels;
    int  rec_prerecord_time;
#if CONFIG_CODEC == SWCODEC
    int  rec_mono_mode;
    int  rec_source_flags;  /* for rec_set_source */
    struct encoder_config enc_config;
#else
    int  rec_quality;
    bool rec_editable;
#endif
};

/* audio recording functions */
void audio_init_recording(void);
void audio_close_recording(void);
void audio_record(const char *filename);
void audio_stop_recording(void);
void audio_pause_recording(void);
void audio_resume_recording(void);
void audio_new_file(const char *filename);
void audio_set_recording_options(struct audio_recording_options *options);
void audio_set_recording_gain(int left, int right, int type);
unsigned long audio_recorded_time(void);
unsigned long audio_num_recorded_bytes(void);

#if CONFIG_CODEC == SWCODEC
/* SWCODEC recording functions */
unsigned long audio_prerecorded_time(void);
#endif /* CONFIG_CODEC == SWCODEC */

#endif /* HAVE_RECORDING */

#if CONFIG_CODEC == SWCODEC
/* SWCODEC misc. audio functions */
#if INPUT_SRC_CAPS != 0
/* audio.c */
void audio_set_input_source(int source, unsigned flags);
/* audio_input_mux: target-specific implementation used by audio_set_source
   to set hardware inputs and audio paths */
void audio_input_mux(int source, unsigned flags);
void audio_set_output_source(int source);
#endif /* INPUT_SRC_CAPS */
#endif /* CONFIG_CODEC == SWCODEC */

#ifdef HAVE_SPDIF_IN
/* returns index into rec_master_sampr_list */
int audio_get_spdif_sample_rate(void);
/* > 0: monitor EBUin, 0: Monitor IISrecv, <0: reset only */
void audio_spdif_set_monitor(int monitor_spdif);
#endif /* HAVE_SPDIF_IN */

#ifdef HAVE_SPEAKER
/* enable/disable the speaker: 0=off, 1=on, 2=on if jack unpluged, off otherwise
 * NOTE this is a one time thing, this function doesn't implement the logic to
 * check for jack events, it merely changes the speaker state to the expected
 * state based on the requested mode.
 */
void audio_enable_speaker(int mode);
#endif

/***********************************************************************/
/* audio event handling */
enum track_event_flags
{
    TEF_NONE      = 0x0,  /* no flags are set */
    TEF_CURRENT   = 0x1,  /* event is for the current track */
#if CONFIG_CODEC == SWCODEC
    TEF_AUTO_SKIP = 0x2,  /* event is sent in context of auto skip */
    TEF_REWIND    = 0x4,  /* interpret as rewind, id3->elapsed is the
                             position before the seek back to 0 */
#endif /* CONFIG_CODEC == SWCODEC */
};

struct track_event
{
    unsigned int flags;   /* combo of enum track_event_flags values */
    struct mp3entry *id3; /* pointer to mp3entry describing track */
};

#if CONFIG_CODEC != SWCODEC
/* subscribe to one or more audio event(s) by OR'ing together the desired */
/* event IDs (defined below); a handler is called with a solitary event ID */
/* (so switch() is okay) and possibly some useful data (depending on the */
/* event); a handler must return one of the return codes defined below */

typedef int (*AUDIO_EVENT_HANDLER)(unsigned short event, unsigned long data);

void audio_register_event_handler(AUDIO_EVENT_HANDLER handler, unsigned short mask);

/***********************************************************************/
/* handler return codes */

#define AUDIO_EVENT_RC_IGNORED      200
    /* indicates that no action was taken or the event was not recognized */

#define AUDIO_EVENT_RC_HANDLED      201
    /* indicates that the event was handled and some action was taken which renders
    the original event invalid; USE WITH CARE!; this return code aborts all further
    processing of the given event */

/***********************************************************************/
/* audio event IDs */

#define AUDIO_EVENT_POS_REPORT      (1<<0)
    /* sends a periodic song position report to handlers; a report is sent on
    each kernal tick; the number of ticks per second is defined by HZ; on each
    report the current song position is passed in 'data'; if a handler takes an
    action that changes the song or the song position it must return
    AUDIO_EVENT_RC_HANDLED which suppresses the event for any remaining handlers */

#define AUDIO_EVENT_END_OF_TRACK    (1<<1)
    /* generated when the end of the currently playing track is reached; no
    data is passed; if the handler implements some alternate end-of-track
    processing it should return AUDIO_EVENT_RC_HANDLED which suppresses the
    event for any remaining handlers as well as the normal end-of-track
    processing */

#endif
#endif
