newoption {
	trigger = "lu_dir",
	value = "path",
	description = "The path to LEGO Universe (Output Directory)"
}

CharacterSaverProjectName = "CharacterSaver"
CharacterSaverStandaloneProjectName = "CharacterSaverStandalone"

AuthPortChangerName = "AuthPortChanger"
AuthPortChangerStandaloneProjectName = "AuthPortChangerStandalone"

CharacterSaverFolder = path.getabsolute("").."\\"

workspace ("Lu_mods")
	targetdir (_OPTIONS["lu_dir"])
	kind ("SharedLib")
	language ("C++")
	conformancemode ("On")
	characterset ("MBCS")
	cppdialect ("c++20")

	configurations {"Debug", "Release" }
	libdirs { "$(TargetDir)", "$(TargetDir)\\.." }
	flags { "MultiProcessorCompile" }

	project (CharacterSaverProjectName)
		targetdir ("build/"..CharacterSaverProjectName)

		files { "dllmain.cpp" }

	-- The standalone version can be run on its own with no other mods. Place in client folder with legouniverse.exe to use.
	project (CharacterSaverStandaloneProjectName)
		targetdir ("build/"..CharacterSaverStandaloneProjectName)

		files { "dllmain.cpp", "Source.def" }
		defines { "STANDALONE=1" }
		targetname ("dinput8")

	project (AuthPortChangerName)
		targetdir ("build/"..AuthPortChangerName)

		files { "dllmainAuthPortChanger.cpp" }

	-- The standalone version can be run on its own with no other mods. Place in client folder with legouniverse.exe to use.
	project (AuthPortChangerStandaloneProjectName)
		targetdir ("build/"..AuthPortChangerStandaloneProjectName)

		files { "dllmainAuthPortChanger.cpp", "Source.def" }
		defines { "STANDALONE=1" }
		targetname ("dinput8")

	project ("*")
	-- Needs to be at the bottom due to scope
	filter ("configurations:Debug")
		symbols ("On")
		optimize ("Off")

		defines {"DEBUG", "_DEBUG", "WIN32_LEAN_AND_MEAN", "_WINDLL"}

	filter ("configurations:Release")
		optimize ("On")

		defines {"NDEBUG", "WIN32_LEAN_AND_MEAN", "_WINDLL"}

