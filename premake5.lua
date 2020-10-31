workspace "i8080-emulator"
        configurations { "Debug", "DebugNoTest", "Release" }

        libdirs {os.findlib("SDL2")}

        kind "ConsoleApp"
        language "C"

	includedirs { "include/" }

project "i8080-emulator"
        targetdir "bin/%{cfg.buildcfg}"

        files { "include/*.h", "src/*.c" }

        filter "configurations:Debug"
                defines { "DEBUG", "_CPU_TEST", "ZAZUSTYLE_DEBUG" }
		buildoptions { "-g" }
                symbols "On"

	filter "configurations:DebugNoTest"
                defines { "DEBUG", "ZAZUSTYLE_DEBUG" }

	filter "configurations:Release"
		defines { "_CPU_TEST" }
		optimize "Speed"
