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

#include "obs-frame-tap.h"

#include <callback/proc.h>
#include <util/bmem.h>
#include <util/threading.h>

typedef struct frame_tap_entry_t {
	frame_tap_cb callback;
	void *param;
} frame_tap_entry_t;

typedef struct frame_tap_registry_t {
	pthread_mutex_t mutex;
	frame_tap_entry_t *entries;
	size_t capacity;
	volatile long count;
} frame_tap_registry_t;

static inline void frame_tap_registry_init(frame_tap_registry_t *reg)
{
	pthread_mutex_init(&reg->mutex, nullptr);
	reg->entries = nullptr;
	reg->capacity = 0;
	os_atomic_set_long(&reg->count, 0);
}

static inline void frame_tap_registry_clear(frame_tap_registry_t *reg)
{
	pthread_mutex_lock(&reg->mutex);
	bfree(reg->entries);
	reg->entries = nullptr;
	reg->capacity = 0;
	os_atomic_set_long(&reg->count, 0);
	pthread_mutex_unlock(&reg->mutex);
}

static inline void frame_tap_registry_destroy(frame_tap_registry_t *reg)
{
	frame_tap_registry_clear(reg);
	pthread_mutex_destroy(&reg->mutex);
}

static inline bool frame_tap_registry_add(frame_tap_registry_t *reg, frame_tap_cb callback, void *param)
{
	if (!callback)
		return false;

	pthread_mutex_lock(&reg->mutex);
	const size_t n = (size_t)os_atomic_load_long(&reg->count);
	for (size_t i = 0; i < n; ++i) {
		if (reg->entries[i].callback == callback && reg->entries[i].param == param) {
			pthread_mutex_unlock(&reg->mutex);
			return false;
		}
	}
	if (n == reg->capacity) {
		const size_t new_cap = reg->capacity ? reg->capacity * 2 : 4;
		reg->entries = (frame_tap_entry_t *)brealloc(reg->entries, new_cap * sizeof(frame_tap_entry_t));
		reg->capacity = new_cap;
	}
	reg->entries[n].callback = callback;
	reg->entries[n].param = param;
	os_atomic_set_long(&reg->count, (long)(n + 1));
	pthread_mutex_unlock(&reg->mutex);
	return true;
}

static inline bool frame_tap_registry_remove(frame_tap_registry_t *reg, frame_tap_cb callback, void *param)
{
	bool removed = false;
	pthread_mutex_lock(&reg->mutex);
	const size_t n = (size_t)os_atomic_load_long(&reg->count);
	for (size_t i = 0; i < n; ++i) {
		if (reg->entries[i].callback == callback && reg->entries[i].param == param) {
			for (size_t j = i + 1; j < n; ++j) {
				reg->entries[j - 1] = reg->entries[j];
			}
			os_atomic_set_long(&reg->count, (long)(n - 1));
			removed = true;
			break;
		}
	}
	pthread_mutex_unlock(&reg->mutex);
	return removed;
}

static inline size_t frame_tap_registry_count(const frame_tap_registry_t *reg)
{
	if (!reg)
		return 0;
	const long c = os_atomic_load_long(&reg->count);
	return c > 0 ? (size_t)c : 0;
}

static inline bool frame_tap_registry_has_consumers(const frame_tap_registry_t *reg)
{
	return frame_tap_registry_count(reg) > 0;
}

static inline void frame_tap_registry_invoke(frame_tap_registry_t *reg, obs_source_t *source,
					     const struct obs_source_frame *frame)
{
	if (os_atomic_load_long(&reg->count) == 0)
		return;

	pthread_mutex_lock(&reg->mutex);
	const size_t n = (size_t)os_atomic_load_long(&reg->count);
	for (size_t i = 0; i < n; ++i) {
		reg->entries[i].callback(reg->entries[i].param, source, frame);
	}
	pthread_mutex_unlock(&reg->mutex);
}

static inline void frame_tap_proc_add(void *data, calldata_t *cd)
{
	auto reg = (frame_tap_registry_t *)data;
	auto callback = (frame_tap_cb)calldata_ptr(cd, "callback");
	void *param = calldata_ptr(cd, "param");
	frame_tap_registry_add(reg, callback, param);
}

static inline void frame_tap_proc_remove(void *data, calldata_t *cd)
{
	auto reg = (frame_tap_registry_t *)data;
	auto callback = (frame_tap_cb)calldata_ptr(cd, "callback");
	void *param = calldata_ptr(cd, "param");
	frame_tap_registry_remove(reg, callback, param);
}

static inline void frame_tap_registry_register_procs(frame_tap_registry_t *reg, proc_handler_t *ph)
{
	if (!ph)
		return;

	proc_handler_add(ph, FRAME_TAP_PROC_ADD_DECL, frame_tap_proc_add, reg);
	proc_handler_add(ph, FRAME_TAP_PROC_REMOVE_DECL, frame_tap_proc_remove, reg);
}
