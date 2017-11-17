/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * *    * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#define LOG_NIDEBUG 0

#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <stdlib.h>

#define LOG_TAG "QCOM PowerHAL"
#include <utils/Log.h>
#include <hardware/hardware.h>
#include <hardware/power.h>

#include "utils.h"
#include "metadata-defs.h"
#include "hint-data.h"
#include "performance.h"
#include "power-common.h"
#include "powerhintparser.h"

static int sustained_mode_handle = 0;
static int vr_mode_handle = 0;
static int launch_handle = 0;
static int sustained_performance_mode = 0;
static int vr_mode = 0;
static int launch_mode = 0;
#define CHECK_HANDLE(x) (((x)>0) && ((x)!=-1))

int is_perf_hint_active(int hint)
{
    switch (hint) {
        case SUSTAINED_PERF_HINT_ID:
            return sustained_performance_mode != 0;
        case VR_MODE_HINT_ID:
            return vr_mode != 0;
        case VR_MODE_SUSTAINED_PERF_HINT_ID:
            return vr_mode != 0 && sustained_performance_mode != 0;
    }
    return 0;
}

static int process_sustained_perf_hint(void *data)
{
    int duration = 0;
    int *resource_values = NULL;
    int resources = 0;

    if (data && sustained_performance_mode == 0) {
        if (vr_mode == 0) { // Sustained mode only.
            resource_values = getPowerhint(SUSTAINED_PERF_HINT_ID, &resources);
            if (!resource_values) {
                ALOGE("Can't get sustained perf hints from xml ");
                return HINT_NONE;
            }
            sustained_mode_handle = interaction_with_handle(
                sustained_mode_handle, duration, resources, resource_values);
            if (!CHECK_HANDLE(sustained_mode_handle)) {
                ALOGE("Failed interaction_with_handle for sustained_mode_handle");
                return HINT_NONE;
            }
        } else if (vr_mode == 1) { // Sustained + VR mode.
            release_request(vr_mode_handle);
            resource_values = getPowerhint(VR_MODE_SUSTAINED_PERF_HINT_ID, &resources);
            if (!resource_values) {
                ALOGE("Can't get VR mode sustained perf hints from xml ");
                return HINT_NONE;
            }
            sustained_mode_handle = interaction_with_handle(
                sustained_mode_handle, duration, resources, resource_values);
            if (!CHECK_HANDLE(sustained_mode_handle)) {
                ALOGE("Failed interaction_with_handle for sustained_mode_handle");
                return HINT_NONE;
            }
        }
        sustained_performance_mode = 1;
    } else if (sustained_performance_mode == 1) {
        release_request(sustained_mode_handle);
        if (vr_mode == 1) { // Switch back to VR Mode.
            resource_values = getPowerhint(VR_MODE_HINT_ID, &resources);
            if (!resource_values) {
                ALOGE("Can't get VR mode perf hints from xml ");
                return HINT_NONE;
            }
            vr_mode_handle = interaction_with_handle(
                vr_mode_handle, duration, resources, resource_values);
            if (!CHECK_HANDLE(vr_mode_handle)) {
                ALOGE("Failed interaction_with_handle for vr_mode_handle");
                return HINT_NONE;
            }
        }
        sustained_performance_mode = 0;
    }
    return HINT_HANDLED;
}

static int process_vr_mode_hint(void *data)
{
    int duration = 0;
    int *resource_values = NULL;
    int resources = 0;

    if (data && vr_mode == 0) {
        if (sustained_performance_mode == 0) { // VR mode only.
            resource_values = getPowerhint(VR_MODE_HINT_ID, &resources);
            if (!resource_values) {
                ALOGE("Can't get VR mode perf hints from xml ");
                return HINT_NONE;
            }
            vr_mode_handle = interaction_with_handle(
                vr_mode_handle, duration, resources, resource_values);
            if (!CHECK_HANDLE(vr_mode_handle)) {
                ALOGE("Failed interaction_with_handle for vr_mode_handle");
                return HINT_NONE;
            }
        } else if (sustained_performance_mode == 1) { // Sustained + VR mode.
            release_request(sustained_mode_handle);
            resource_values = getPowerhint(VR_MODE_SUSTAINED_PERF_HINT_ID, &resources);
            if (!resource_values) {
                ALOGE("Can't get VR mode sustained perf hints from xml ");
                return HINT_NONE;
            }
            vr_mode_handle = interaction_with_handle(
                vr_mode_handle, duration, resources, resource_values);
            if (!CHECK_HANDLE(vr_mode_handle)) {
                ALOGE("Failed interaction_with_handle for vr_mode_handle");
                return HINT_NONE;
            }
        }
        vr_mode = 1;
    } else if (vr_mode == 1) {
        release_request(vr_mode_handle);
        if (sustained_performance_mode == 1) { // Switch back to sustained Mode.
            resource_values = getPowerhint(SUSTAINED_PERF_HINT_ID, &resources);
            if (!resource_values) {
                ALOGE("Can't get sustained perf hints from xml ");
                return HINT_NONE;
            }
            sustained_mode_handle = interaction_with_handle(
                sustained_mode_handle, duration, resources, resource_values);
            if (!CHECK_HANDLE(sustained_mode_handle)) {
                ALOGE("Failed interaction_with_handle for sustained_mode_handle");
                return HINT_NONE;
            }
        }
        vr_mode = 0;
    }

    return HINT_HANDLED;
}

static int process_boost(int hint_id, int boost_handle, int duration)
{
    int *resource_values;
    int resources;

    resource_values = getPowerhint(hint_id, &resources);

    if (resource_values != NULL) {
        boost_handle = interaction_with_handle(
            boost_handle, duration, resources, resource_values);
        if (!CHECK_HANDLE(boost_handle)) {
            ALOGE("Failed interaction_with_handle for hint_id %d", hint_id);
        }
    }

    return boost_handle;
}

static int process_video_encode_hint(void *metadata)
{
    char governor[80];
    struct video_encode_metadata_t video_encode_metadata;
    static int boost_handle = -1;

    if(!metadata)
       return HINT_NONE;

    if (get_scaling_governor(governor, sizeof(governor)) == -1) {
        ALOGE("Can't obtain scaling governor.");

        return HINT_NONE;
    }

    /* Initialize encode metadata struct fields */
    memset(&video_encode_metadata, 0, sizeof(struct video_encode_metadata_t));
    video_encode_metadata.state = -1;
    video_encode_metadata.hint_id = DEFAULT_VIDEO_ENCODE_HINT_ID;

    if (parse_video_encode_metadata((char *)metadata, &video_encode_metadata) ==
            -1) {
       ALOGE("Error occurred while parsing metadata.");
       return HINT_NONE;
    }

    if (video_encode_metadata.state == 1) {
        int duration = 2000; // boosts 2s for starting encoding
        boost_handle = process_boost(BOOST_HINT_ID, boost_handle, duration);
        ALOGD("LAUNCH ENCODER-ON: %d MS", duration);
        if (is_interactive_governor(governor)) {

            int *resource_values;
            int resources;

            /* extract perflock resources */
            resource_values = getPowerhint(video_encode_metadata.hint_id, &resources);

            if (resource_values != NULL)
               perform_hint_action(video_encode_metadata.hint_id, resource_values, resources);
            ALOGI("Video Encode hint start");
            return HINT_HANDLED;
        } else {
            return HINT_HANDLED;
        }
    } else if (video_encode_metadata.state == 0) {
        if (is_interactive_governor(governor)) {
            undo_hint_action(video_encode_metadata.hint_id);
            ALOGI("Video Encode hint stop");
            return HINT_HANDLED;
        }
    }
    return HINT_NONE;
}

int process_camera_launch_hint(int32_t duration)
{
    static int cam_launch_handle = -1;

    if (duration > 0) {
        cam_launch_handle = process_boost(CAMERA_LAUNCH_HINT_ID, cam_launch_handle, duration);
        ALOGD("CAMERA LAUNCH ON: %d MS", duration);
        return HINT_HANDLED;
    } else if (duration == 0) {
        release_request(cam_launch_handle);
        ALOGD("CAMERA LAUNCH OFF");
        return HINT_HANDLED;
    } else {
        ALOGE("CAMERA LAUNCH INVALID DATA: %d", duration);
    }
    return HINT_NONE;
}

int process_camera_streaming_hint(int32_t duration)
{
    static int cam_streaming_handle = -1;

    if (duration > 0) {
        cam_streaming_handle = process_boost(CAMERA_STREAMING_HINT_ID, cam_streaming_handle, duration);
        ALOGD("CAMERA STREAMING ON: %d MS", duration);
        return HINT_HANDLED;
    } else if (duration == 0) {
        release_request(cam_streaming_handle);
        ALOGD("CAMERA STREAMING OFF");
        return HINT_HANDLED;
    } else {
        ALOGE("CAMERA STREAMING INVALID DATA: %d", duration);
    }
    return HINT_NONE;
}

int process_camera_shot_hint(int32_t duration)
{
    static int cam_shot_handle = -1;

    if (duration > 0) {
        cam_shot_handle = process_boost(CAMERA_SHOT_HINT_ID, cam_shot_handle, duration);
        ALOGD("CAMERA SHOT ON: %d MS", duration);
        return HINT_HANDLED;
    } else if (duration == 0) {
        release_request(cam_shot_handle);
        ALOGD("CAMERA SHOT OFF");
        return HINT_HANDLED;
    } else {
        ALOGE("CAMERA SHOT INVALID DATA: %d", duration);
    }
    return HINT_NONE;
}

int process_audio_streaming_hint(int32_t duration)
{
    static int audio_streaming_handle = -1;

    if (duration > 0) {
        // set max duration 2s for starting audio
        audio_streaming_handle = process_boost(AUDIO_STREAMING_HINT_ID, audio_streaming_handle, 2000);
        ALOGD("AUDIO STREAMING ON");
        return HINT_HANDLED;
    } else if (duration == 0) {
        release_request(audio_streaming_handle);
        ALOGD("AUDIO STREAMING OFF");
        return HINT_HANDLED;
    } else {
        ALOGE("AUDIO STREAMING INVALID DATA: %d", duration);
    }
    return HINT_NONE;
}

int process_audio_low_latency_hint(int32_t data)
{
    static int audio_low_latency_handle = -1;

    if (data) {
        // Hint until canceled
        audio_low_latency_handle = process_boost(AUDIO_LOW_LATENCY_HINT_ID, audio_low_latency_handle, 0);
        ALOGD("AUDIO LOW LATENCY ON");
    } else {
        release_request(audio_low_latency_handle);
        ALOGD("AUDIO LOW LATENCY OFF");
        return HINT_HANDLED;
    }
    return HINT_HANDLED;
}

static int process_activity_launch_hint(void *data)
{
    // boost will timeout in 5s
    int duration = 5000;
    if (sustained_performance_mode || vr_mode) {
        return HINT_HANDLED;
    }

    ALOGD("LAUNCH HINT: %s", data ? "ON" : "OFF");
    // restart the launch hint if the framework has not yet released
    // this shouldn't happen, but we've seen bugs where it could
    if (data) {
        launch_handle = process_boost(BOOST_HINT_ID, launch_handle, duration);
        if (launch_handle > 0) {
            launch_mode = 1;
            ALOGI("Activity launch hint handled");
            return HINT_HANDLED;
        } else {
            return HINT_NONE;
        }
    } else if (data == NULL  && launch_mode == 1) {
        release_request(launch_handle);
        launch_mode = 0;
        return HINT_HANDLED;
    }
    return HINT_NONE;
}

int power_hint_override(power_hint_t hint, void *data)
{
    int ret_val = HINT_NONE;
    switch(hint) {
        case POWER_HINT_VIDEO_ENCODE:
            ret_val = process_video_encode_hint(data);
            break;
        case POWER_HINT_SUSTAINED_PERFORMANCE:
            ret_val = process_sustained_perf_hint(data);
            break;
        case POWER_HINT_VR_MODE:
            ret_val = process_vr_mode_hint(data);
            break;
        case POWER_HINT_LAUNCH:
            ret_val = process_activity_launch_hint(data);
            break;
        default:
            break;
    }
    return ret_val;
}

int set_interactive_override(int UNUSED(on))
{
    return HINT_HANDLED; /* Don't excecute this code path, not in use */
}
