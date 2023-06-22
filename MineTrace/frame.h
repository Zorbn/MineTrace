#pragma once

#include <inttypes.h>

struct Frame {
	int32_t width;
	int32_t height;
	int32_t window_width;
	int32_t window_height;
	int32_t min_window_width;
	int32_t min_window_height;
	uint32_t* pixels;
};