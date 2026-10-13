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

	std::printf("test-source-decision: ALL ASSERTIONS PASSED\n");
	return 0;
}
