#include "winraii.hpp"

struct injector {
	static bool inject(DWORD pid, const char* filename) {
		handle proc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
		if (!proc) return false;

		virtual_memory mem(proc.get(), strlen(filename));
		if (!mem) return false;

		if (!mem.write(filename)) return false;

		auto load_lib = (LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandle("KERNEL32.DLL"), "LoadLibraryA");
		handle thread = CreateRemoteThread(proc.get(), nullptr, 0, load_lib, mem.get(), 0, nullptr);
		WaitForSingleObject(thread.get(), INFINITE);

		return (bool)thread;
	}
};
