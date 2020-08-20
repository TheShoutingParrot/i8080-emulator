workspace "invadersemu"
        configurations { "Debug"}

        libdirs {os.findlib("SDL2")}

        kind "ConsoleApp"
        language "C"

	includedirs { "include/" }

project "invadersemu"
        targetdir "bin/%{cfg.buildcfg}"

        files { "include/*.h", "src/*.c" }

        filter "configurations:Debug"
                defines { "DEBUG", "_CPU_TEST", "ZAZUSTYLE_DEBUG" }
		buildoptions { "-g" }
                symbols "On"
