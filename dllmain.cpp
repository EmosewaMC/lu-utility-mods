// I don't think we really need to care about _s functions.
#define _CRT_SECURE_NO_WARNINGS

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

// Credits to lcdr and Jett for this snippet.
inline void patch_protect(size_t address, const void* data, size_t size) {
	DWORD oldProtect;
	VirtualProtect((void*)address, size, PAGE_EXECUTE_READWRITE, &oldProtect);
	memcpy((void*)address, data, size);
	VirtualProtect((void*)address, size, oldProtect, &oldProtect);
}

int PatchFunction(uint32_t ghidraAddress, uint32_t func) {
	enum { OPCODE_CALL = 0xE8 };
	int count = 0;

	TCHAR szSelf[MAX_PATH]; GetModuleFileName(NULL, szSelf, MAX_PATH);
	HMODULE mod = GetModuleHandle(szSelf);
	if (!mod) return 0;
	MODULEINFO mi;
	GetModuleInformation(GetCurrentProcess(), mod, &mi, sizeof(mi));
	DWORD moduleSize = mi.SizeOfImage;

	DWORD begin = (DWORD)mi.lpBaseOfDll;
	DWORD end = begin + moduleSize;


	// Iterate through the whole exe in memory
	for (uint8_t* it = (uint8_t*)mod; it < (uint8_t*)end; ++it) {
		try {
			if (*it != OPCODE_CALL) continue;
		} catch (...) {
			continue;
		}
		// Get the address
		int32_t* givenOffset = (int32_t*)(it + 1);

		uint32_t givenAddress = ((int32_t)it) + 5 + *givenOffset;

		// Not our target address :(
		if (givenAddress != ghidraAddress) continue;

		// Calculate new offset
		int32_t newOffset = ((int32_t)func) - ((int32_t)it + 5);

		// Update the instructions
		patch_protect((size_t)it + 1, &newOffset, 4);
		++count;
	}
	return count;
}

#pragma pack(1)
struct Packet {
	short connectionType;
	int messageType;
	char padding;
	int skip;
	unsigned char compressed;
	union {
		int numberOfBytesInData;
		unsigned char* dataStart;
	} dataUnion;
	int sizeOfCompressedData;
	unsigned char dataStart;
};

#pragma warning(disable:4244)

struct T {
	void f(Packet* packet, int __, int ___) {
		TCHAR szSelf[MAX_PATH];
		GetModuleFileName(NULL, szSelf, MAX_PATH);
		uintptr_t mod = (uintptr_t)GetModuleHandle(szSelf);
		try {
			std::optional<std::unique_ptr<unsigned char[]>> zdata;
			unsigned char* lnv = nullptr;
			if (packet->compressed) {
				zdata = std::make_unique<unsigned char[]>(packet->dataUnion.numberOfBytesInData);

				// Decompress the packet data since on dlu its decompresssed by default

				typedef int(__cdecl* ZLIB_DECOMPRESS)(unsigned char*, int*, unsigned char*, int);

				auto zlibAddr = 0x00367960 + mod;
				auto zlibAddrFn = (ZLIB_DECOMPRESS)zlibAddr;
				int ret = zlibAddrFn(zdata.value().get(), &packet->dataUnion.numberOfBytesInData, &packet->dataStart, packet->sizeOfCompressedData);
				lnv = zdata.value().get();
			} else {
				lnv = packet->dataUnion.dataStart;
			}

			std::string characterName;
			std::string saveData;

			unsigned char* curOffset = lnv;
			auto numKeys = (int)*curOffset;
			curOffset += 4;
			for (int i = 0; i < numKeys; i++) {
				auto keyLen = *reinterpret_cast<uint8_t*>(curOffset);
				curOffset += 1;
				std::wstring key((const wchar_t*)curOffset, keyLen / 2);
				curOffset += keyLen;
				auto valueType = *reinterpret_cast<uint8_t*>(curOffset);
				curOffset += 1;
				switch (valueType) {
					case 1:
					case 3:
					case 5:
						curOffset += 4;
						break;
					case 4:
					case 8:
					case 9:
						curOffset += 8;
						break;
					case 7:
						curOffset += 1;
						break;
					case 0:
					{
						uint32_t valueLength = *reinterpret_cast<uint32_t*>(curOffset);
						curOffset += 4;
						std::wstring valueStr((const wchar_t*)curOffset, valueLength);
						curOffset += valueLength * 2;
						if (key == L"name") {
							characterName.assign(valueStr.begin(), valueStr.end());
						}
						break;
					}
					case 13:
					{
						uint32_t valueLength2 = 0;
						valueLength2 = *reinterpret_cast<uint32_t*>(curOffset);
						curOffset += 4;
						std::string valueStr((const char*)curOffset, valueLength2);
						curOffset += valueLength2;
						if (key == L"xmlData") {
							saveData = valueStr;
						}
						break;
					}
				}
			}

			std::ifstream bootcfg("../boot.cfg"); // boot.cfg path 

			// find last occurance of AUTHSERVERIP since that is the one which takes precedence
			std::string line;
			std::string serverIp = "localhost";
			while (std::getline(bootcfg, line)) {
				if (line.find("AUTHSERVERIP") != std::string::npos) {
					serverIp = line.substr(strlen("AUTHSERVERIP") + 3);
					if (!serverIp.empty()) serverIp.pop_back();
				}
			}

			// Write save data to the characters setting folder and then a subfolder with the server name.
			CHAR localAppDataPath[MAX_PATH];
			SHGetFolderPath(0, CSIDL_LOCAL_APPDATA, 0, 0, localAppDataPath);
			std::filesystem::path playerFolder = localAppDataPath;
			playerFolder = playerFolder / "LEGO Software" / "LEGO Universe" / characterName / serverIp / "CharacterSave.xml";
			if (!std::filesystem::exists(playerFolder.parent_path())) std::filesystem::create_directories(playerFolder.parent_path());
			std::ofstream saveFile(playerFolder);
			saveFile << saveData;
		} catch (const std::exception& e) {
			(void)e;
		} catch (...) {

		}

		// Call the original function
		auto addr = 0x0072eb40 + mod;
		typedef bool(__thiscall* LC_CUR_FNC)(T*, unsigned char*, int, int);
		auto ptrLC_CUR_FNC = (LC_CUR_FNC)addr;
		ptrLC_CUR_FNC(this, *(unsigned char**)&packet, __, ___);
	}
};

#ifdef STANDALONE_CHARACTER_SAVER

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

#endif // STANDALONE_CHARACTER_SAVER

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved) {
	switch (ul_reason_for_call) {
		case DLL_PROCESS_ATTACH:
		{
			TCHAR szSelf[MAX_PATH];
			GetModuleFileName(NULL, szSelf, MAX_PATH);
			HMODULE mod = GetModuleHandle(szSelf);
			auto s = &T::f;
			PatchFunction(0x0072eb40 + (uintptr_t)mod, (uint32_t) * (void**)&s);
		}
	}

	return TRUE;
}
