#include <inttypes.h>
#include <stdlib.h>
#include <Windows.h>
#include <winnt.h>
#include <sys/timeb.h>
#include <stdio.h>
#include <stdbool.h>
#include <time.h>

#include "game.h"

LRESULT CALLBACK window_process_message(HWND window_handle, UINT message, WPARAM w_param, LPARAM l_param);

struct State {
	BITMAPINFO bitmap_info;
	HBITMAP bitmap;
	HDC device_context;
	HBRUSH background_brush;
	bool quit;

	struct Frame frame;
};

const int32_t default_window_width = 320;
const int32_t default_window_height = 240;

int WINAPI WinMain(_In_ HINSTANCE h_instance, _In_opt_ HINSTANCE h_prev_instance, _In_ PSTR p_cmd_line, _In_ int n_cmd_show) {
	const wchar_t window_class_name[] = L"Window Class";
	WNDCLASS window_class = { 0 };
	window_class.lpfnWndProc = window_process_message;
	window_class.hInstance = h_instance;
	window_class.lpszClassName = window_class_name;
	RegisterClass(&window_class);

	static struct State state = { 0 };
	state.frame.width = default_window_width;
	state.frame.height = default_window_height;

	state.bitmap_info.bmiHeader.biWidth = state.frame.width;
	state.bitmap_info.bmiHeader.biHeight = state.frame.height;
	state.bitmap_info.bmiHeader.biSize = sizeof(state.bitmap_info.bmiHeader);
	state.bitmap_info.bmiHeader.biPlanes = 1;
	state.bitmap_info.bmiHeader.biBitCount = 32;
	state.bitmap_info.bmiHeader.biCompression = BI_RGB;
	state.device_context = CreateCompatibleDC(0);
	state.background_brush = CreateSolidBrush(BLACK_BRUSH);

	state.bitmap = CreateDIBSection(NULL, &state.bitmap_info, DIB_RGB_COLORS, (void**)&state.frame.pixels, 0, 0);

	if (!state.bitmap) {
		return -1;
	}

	SelectObject(state.device_context, state.bitmap);

	RECT window_size = { 0 };
	window_size.right = state.frame.width;
	window_size.bottom = state.frame.height;
	AdjustWindowRect(&window_size, WS_OVERLAPPEDWINDOW, false);
	state.frame.min_window_width = window_size.right - window_size.left;
	state.frame.min_window_height = window_size.bottom - window_size.top;

	HWND window_handle = CreateWindowEx(0, window_class_name, L"Window", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
		window_size.right - window_size.left, window_size.bottom - window_size.top, NULL, NULL, h_instance, NULL);

	if (window_handle == NULL) {
		return -1;
	}

	SetWindowLongPtr(window_handle, GWLP_USERDATA, (LONG_PTR)&state);
	ShowWindow(window_handle, n_cmd_show);

	MSG message = { 0 };

	struct Game game = { 0 };

	clock_t lastTime = clock();

	char str[256] = { 0 };
	float timer = 0;

	start(&game);

	while (!state.quit) {
		while (PeekMessage(&message, NULL, 0, 0, PM_REMOVE)) {
			DispatchMessage(&message);
		}

		if (state.frame.pixels) {
			draw(&game, &state.frame);
		}

		InvalidateRect(window_handle, NULL, FALSE);
		UpdateWindow(window_handle);

		clock_t currentTime = clock();
		float deltaTime = (float)(currentTime - lastTime) / CLOCKS_PER_SEC * 1000.0f;
		lastTime = currentTime;
		timer += deltaTime;
		if (timer > 5.0f) {
			sprintf_s(str, sizeof(str), "ms: %f\n", deltaTime);
			SetWindowTextA(window_handle, (LPCSTR)str);
			timer = 0.0f;
		}
	}

	end(&game);

	DeleteObject(state.background_brush);

	return 0;
}

LRESULT CALLBACK window_process_message(HWND window_handle, UINT message, WPARAM w_param, LPARAM l_param) {
	static struct State* state;

	if (!state) {
		state = (struct State*)GetWindowLongPtr(window_handle, GWLP_USERDATA);
	}

	switch (message) {

	case WM_CREATE: {

	} break;

	case WM_QUIT:
	case WM_DESTROY: {
		state->quit = true;
	} break;

	case WM_PAINT: {
		static PAINTSTRUCT paint;
		static HDC device_context;

		device_context = BeginPaint(window_handle, &paint);

		RECT scaled_window = { 0 };

		SelectObject(device_context, state->background_brush);

		if (state->frame.window_width < state->frame.window_height) {
			scaled_window.right = state->frame.window_width;
			scaled_window.bottom = (LONG)((state->frame.height / (float)state->frame.width) * scaled_window.right);
			scaled_window.top = (state->frame.window_height - scaled_window.bottom) / 2;
			scaled_window.bottom += scaled_window.top;

			Rectangle(device_context, paint.rcPaint.left, 0, paint.rcPaint.right, scaled_window.top);
			Rectangle(device_context, paint.rcPaint.left, scaled_window.bottom, paint.rcPaint.right, paint.rcPaint.bottom);
		}
		else {
			scaled_window.bottom = state->frame.window_height;
			scaled_window.right = (LONG)((state->frame.width / (float)state->frame.height) * scaled_window.bottom);
			scaled_window.left = (state->frame.window_width - scaled_window.right) / 2;
			scaled_window.right += scaled_window.left;

			Rectangle(device_context, 0, paint.rcPaint.top, scaled_window.left, paint.rcPaint.bottom);
			Rectangle(device_context, scaled_window.right, paint.rcPaint.top, paint.rcPaint.right, paint.rcPaint.bottom);
		}

		StretchBlt(device_context, scaled_window.left, scaled_window.top, scaled_window.right - scaled_window.left,
			scaled_window.bottom - scaled_window.top, state->device_context, 0, 0, state->frame.width, state->frame.height, SRCCOPY);
		EndPaint(window_handle, &paint);

		ValidateRect(window_handle, NULL);
	} break;

	case WM_SIZE: {
		state->frame.window_width = LOWORD(l_param);
		state->frame.window_height = HIWORD(l_param);
	} break;

	case WM_GETMINMAXINFO: {
		LPMINMAXINFO lp_mmi = (LPMINMAXINFO)l_param;

		if (state) {
			lp_mmi->ptMinTrackSize.x = state->frame.min_window_width;
			lp_mmi->ptMinTrackSize.y = state->frame.min_window_height;
		}
	} break;

	default: {
		return DefWindowProc(window_handle, message, w_param, l_param);
	} break;

	}

	return 0;
}