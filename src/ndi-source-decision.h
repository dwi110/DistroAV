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

#ifdef __cplusplus
}
#endif
