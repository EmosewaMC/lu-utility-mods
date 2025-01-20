
#include <windows.h>
#include <cstdint>
#include <Psapi.h>
#include <fstream>
#include <memory>
#include <ranges>
#include <vector>
#include <optional>
#include <filesystem>
#include <shlobj_core.h>
#include <Unknwnbase.h>

// credit to lcdr and jett
inline void patch_protect(size_t address, const void* data, size_t size) {
	DWORD oldProtect;
	VirtualProtect((void*)address, size, PAGE_EXECUTE_READWRITE, &oldProtect);
	memcpy((void*)address, data, size);
	VirtualProtect((void*)address, size, oldProtect, &oldProtect);
}

void __stdcall ModMain() {
	std::ifstream bootcfg("boot.cfg"); // boot.cfg path 

	// find last occurance of AUTHSERVERPORT since that is the one which takes precedence
	uint16_t port = 1001;
	std::string line;
	std::string portStr = "localhost";
	while (std::getline(bootcfg, line)) {
		if (line.find("AUTHSERVERPORT") != std::string::npos) {
			portStr = line.substr(strlen("AUTHSERVERPORT") + 3);
			if (!portStr.empty() && !portStr.starts_with('#')) portStr.pop_back();
		}
	}

	try {
		port = static_cast<uint16_t>(std::stoul(portStr));
	} catch (...) {

	}

	TCHAR szSelf[MAX_PATH];
	GetModuleFileName(NULL, szSelf, MAX_PATH);
	uintptr_t mod = (uintptr_t)GetModuleHandle(szSelf);

	auto addr = (void*)(0x00b2e442 - 0x00400000 + mod);

	patch_protect((size_t)addr, &port, 4);

	addr = (void*)(0x00b2e3b6 - 0x00400000 + mod);

	patch_protect((size_t)addr, &port, 4);
}

#ifdef STANDALONE

#define CALLPROC(function, ...) reinterpret_cast<decltype(function) *>(GetProcAddress(dinput8, #function))(__VA_ARGS__);

namespace {
	HINSTANCE dinput8 = NULL;
};

void Startup() {
	char path[MAX_PATH];
	GetSystemDirectory(path, MAX_PATH);
	strcat(path, "\\DInput8.dll");

	dinput8 = LoadLibrary(path);

	if (!dinput8) {
		MessageBox(NULL, "Cannot load system DInput8.dll", "Error", MB_ICONERROR | MB_OK);
		std::exit(1);
	}
}

extern "C" {
	STDAPI DirectInput8Create(HINSTANCE hinst, DWORD dwVersion, IID const& riidltf, LPVOID* ppvOut, LPUNKNOWN punkOuter) {
		Startup();
		return CALLPROC(DirectInput8Create, hinst, dwVersion, riidltf, ppvOut, punkOuter);
	}
}

#endif // STANDALONE

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved) {
	switch (ul_reason_for_call) {
	case DLL_PROCESS_ATTACH:  ModMain();
	}

	return TRUE;
}
