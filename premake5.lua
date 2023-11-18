workspace "spy_ws"
configurations { "main" }
project "spy"
	kind "ConsoleApp"
	language "C++"
	files {
		"*.hpp",
		"*.h",
		"*.cpp",
		"*.c"
	}
	optimize "On"
	flags {
		"WinMain"
	}
	cppdialect "C++20"
