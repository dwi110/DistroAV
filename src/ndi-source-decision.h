/******************************************************************************
	Copyright (C) 2016-2024 DistroAV <contact@distroav.org>

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, see <https://www.gnu.org/licenses/>.
******************************************************************************/

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef PROP_BEHAVIOR_KEEP_ACTIVE
#define PROP_BEHAVIOR_KEEP_ACTIVE 0
#define PROP_BEHAVIOR_STOP_RESUME_BLANK 1
#define PROP_BEHAVIOR_STOP_RESUME_LAST_FRAME 2
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * DistroAV NDI source receiver lifecycle and frame dispatch decisions.
 *
 * When the source has at least one frame-tap consumer (e.g. replay-source buffer),
 * it behaves like Keep Active for the receiver lifecycle so hidden cameras continue
 * buffering, while suppressing obs_source_output_video to avoid unnecessary OBS
 * rendering / GPU texture upload overhead.
 */

static inline bool should_receive(bool showing, size_t consumers)
{
	return showing || (consumers > 0);
}

static inline bool should_output_video(bool showing)
{
	return showing;
}

static inline bool keep_thread_alive(int behavior, bool active, bool showing, size_t consumers)
{
	if (consumers > 0)
		return true;
	if (behavior == PROP_BEHAVIOR_KEEP_ACTIVE)
		return true;
	return active || showing;
}

/*
 * Fast reconnect after the sender closed the NDI session.
 *
 * PTZOptics Move SE cameras end their NDI session on a fixed schedule (TCP FIN). The NDI SDK (6.3.2) only reconnects
 * every ~16 s, so the picture stalls 16 s, or 32 s when the camera still refuses the first SDK retry. The camera
 * accepts a new receiver either at once or after 16-21 s, closing refused sessions within 30-60 ms. A new receiver
 * per attempt brings the picture back within ~1 s of the camera accepting again.
 *
 * Called only while the receiver has no connection. last_video_ns is 0 until the source delivered video (the SDK
 * handles the initial connect), attempt_ns is 0 until the first attempt after the loss. attempt_connected tells
 * whether the current attempt's receiver was connected at some point.
 *
 * A refused session (connected, then closed by the camera) is retried after 1 s, after 60 s without video every 5 s.
 * A receiver that has not connected yet is replaced only after 60 s. A receiver created by NDI name depends on NDI
 * discovery inside the OBS process; on 2026-10-20 that discovery listed no remote source any more and name-only
 * receivers stayed unconnected for over 50 minutes, while a receiver with the source URL connected at once. So
 * reconnect receivers use the URL remembered from discovery (ndi-finder), and after 60 s without a connection the
 * next receiver switches between URL and name only, in case the remembered URL is stale.
 */
#define RECONNECT_FIRST_DELAY_NS 500000000ULL
#define RECONNECT_RETRY_NS 1000000000ULL
#define RECONNECT_SLOW_RETRY_NS 5000000000ULL
#define RECONNECT_SLOW_AFTER_NS 60000000000ULL
#define RECONNECT_PENDING_SWITCH_NS 60000000000ULL

static inline bool should_reconnect(uint64_t now_ns, uint64_t last_video_ns, uint64_t attempt_ns,
				    bool attempt_connected)
{
	if (last_video_ns == 0 || now_ns < last_video_ns)
		return false;
	if (attempt_ns == 0)
		return now_ns - last_video_ns >= RECONNECT_FIRST_DELAY_NS;
	if (now_ns < attempt_ns)
		return false;
	if (!attempt_connected)
		return now_ns - attempt_ns >= RECONNECT_PENDING_SWITCH_NS;
	const uint64_t wait_ns = now_ns - last_video_ns >= RECONNECT_SLOW_AFTER_NS ? RECONNECT_SLOW_RETRY_NS
										   : RECONNECT_RETRY_NS;
	return now_ns - attempt_ns >= wait_ns;
}

#ifdef __cplusplus
}
#endif
