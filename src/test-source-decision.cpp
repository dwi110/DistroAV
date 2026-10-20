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

#include "ndi-source-decision.h"
#include <cassert>
#include <cstdio>

int main()
{
	// 1. should_receive(showing, consumers)
	// When showing, always receive regardless of consumers
	assert(should_receive(true, 0) == true);
	assert(should_receive(true, 1) == true);
	assert(should_receive(true, 5) == true);

	// When hidden, receive if and only if there are frame-tap consumers
	assert(should_receive(false, 0) == false); // Avoid busy-waiting, no frames
	assert(should_receive(false, 1) == true);  // Hidden but buffering for replay!
	assert(should_receive(false, 3) == true);

	// 2. should_output_video(showing)
	// Video is only output to OBS when source is actually showing
	assert(should_output_video(true) == true);
	assert(should_output_video(false) == false); // Suppress async copies/uploads for hidden cameras

	// 3. keep_thread_alive(behavior, active, showing, consumers)
	// If consumers > 0, always keep thread alive (behaves like Keep Active)
	assert(keep_thread_alive(PROP_BEHAVIOR_STOP_RESUME_LAST_FRAME, false, false, 1) == true);
	assert(keep_thread_alive(PROP_BEHAVIOR_STOP_RESUME_BLANK, false, false, 1) == true);
	assert(keep_thread_alive(PROP_BEHAVIOR_KEEP_ACTIVE, false, false, 1) == true);
	assert(keep_thread_alive(PROP_BEHAVIOR_STOP_RESUME_LAST_FRAME, false, false, 2) == true);

	// If Keep Active configured, keep thread alive even when inactive/hidden with 0 consumers
	assert(keep_thread_alive(PROP_BEHAVIOR_KEEP_ACTIVE, false, false, 0) == true);
	assert(keep_thread_alive(PROP_BEHAVIOR_KEEP_ACTIVE, true, true, 0) == true);

	// Normal behavior with 0 consumers: alive if active or showing
	assert(keep_thread_alive(PROP_BEHAVIOR_STOP_RESUME_LAST_FRAME, true, false, 0) == true);
	assert(keep_thread_alive(PROP_BEHAVIOR_STOP_RESUME_LAST_FRAME, false, true, 0) == true);
	assert(keep_thread_alive(PROP_BEHAVIOR_STOP_RESUME_LAST_FRAME, true, true, 0) == true);

	// Normal behavior with 0 consumers: stop thread when inactive AND hidden
	assert(keep_thread_alive(PROP_BEHAVIOR_STOP_RESUME_LAST_FRAME, false, false, 0) == false);
	assert(keep_thread_alive(PROP_BEHAVIOR_STOP_RESUME_BLANK, false, false, 0) == false);

	// Transition: last consumer removed while source is hidden and inactive
	// Before remove (1 consumer): alive
	assert(keep_thread_alive(PROP_BEHAVIOR_STOP_RESUME_LAST_FRAME, false, false, 1) == true);
	// After remove (0 consumers): restored to configured behavior -> stop!
	assert(keep_thread_alive(PROP_BEHAVIOR_STOP_RESUME_LAST_FRAME, false, false, 0) == false);

	// Transition: last consumer removed while source is visible or active
	// Still kept alive because showing/active
	assert(keep_thread_alive(PROP_BEHAVIOR_STOP_RESUME_LAST_FRAME, false, true, 0) == true);
	assert(keep_thread_alive(PROP_BEHAVIOR_STOP_RESUME_LAST_FRAME, true, false, 0) == true);

	// 4. should_reconnect(now, last_video, attempt, attempt_connected)
	const uint64_t s = 1000000000ULL;
	const uint64_t ms = 1000000ULL;
	const uint64_t t0 = 1000 * s; // last video frame

	// Never received video: the SDK handles the initial connect, no fast reconnect
	assert(should_reconnect(t0, 0, 0, false) == false);
	assert(should_reconnect(t0 + 100 * s, 0, 0, false) == false);

	// First attempt 0.5 s after the last video frame
	assert(should_reconnect(t0 + 499 * ms, t0, 0, false) == false);
	assert(should_reconnect(t0 + 500 * ms, t0, 0, false) == true);

	// Camera refused the attempt (connected, then closed): retry after 1 s
	const uint64_t a1 = t0 + 500 * ms;
	assert(should_reconnect(a1 + 999 * ms, t0, a1, true) == false);
	assert(should_reconnect(a1 + 1 * s, t0, a1, true) == true);

	// Attempt not connected yet (NDI name still resolving): never replaced, the SDK connects it when resolved.
	// Replacing it every 5 s restarted the ~4-s name lookup and kept three cameras dark for minutes (2026-10-20).
	assert(should_reconnect(a1 + 1 * s, t0, a1, false) == false);
	assert(should_reconnect(a1 + 5 * s, t0, a1, false) == false);
	assert(should_reconnect(a1 + 600 * s, t0, a1, false) == false);

	// After 60 s without video a refused attempt waits 5 s
	const uint64_t late = t0 + 60 * s;
	assert(should_reconnect(late + 1 * s, t0, late, true) == false);
	assert(should_reconnect(late + 4999 * ms, t0, late, true) == false);
	assert(should_reconnect(late + 5 * s, t0, late, true) == true);
	// ... and the switch happens at exactly 60 s since the last frame
	const uint64_t a2 = t0 + 58 * s;
	assert(should_reconnect(t0 + 59 * s, t0, a2, true) == true);
	assert(should_reconnect(t0 + 60 * s, t0, a2, true) == false);

	// Clock values before the reference points never trigger (no unsigned wrap)
	assert(should_reconnect(t0 - 1, t0, 0, false) == false);
	assert(should_reconnect(a1 - 1, t0, a1, true) == false);

	std::printf("test-source-decision: ALL ASSERTIONS PASSED\n");
	return 0;
}
