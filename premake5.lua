workspace "i8080-emulator"
        configurations { "Debug", "DebugNoTest", "Release" }

        kind "ConsoleApp"
        language "C"

	includedirs { "include/" }

project "i8080-emulator"
        targetdir "bin/%{cfg.buildcfg}"

        files { "include/*.h", "src/*.c" }

        filter "configurations:Debug"
                defines { "DEBUG", "_CPU_TEST" }
		buildoptions { "-g" }
                symbols "On"

	filter "configurations:SingleStepDebug"
                defines { "DEBUG", "_CPU_TEST", "SINGLE_STEP" }
		buildoptions { "-g" }
                symbols "On"

	filter "configurations:Release"
		defines { "_CPU_TEST" }
		optimize "Speed"
