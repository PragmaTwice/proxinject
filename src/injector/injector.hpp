#include "winraii.hpp"
#include <filesystem>

namespace fs = std::filesystem;

struct injector {
	static inline const FARPROC load_library = GetProcAddress(GetModuleHandle("kernel32.dll"), "LoadLibraryW");

	static bool inject(DWORD pid, std::wstring_view filename) {
		handle proc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
		if (!proc) return false;

		virtual_memory mem(proc.get(), (filename.size() + 1) * sizeof(wchar_t));
		if (!mem) return false;

		if (!mem.write(filename.data())) return false;

		handle thread = CreateRemoteThread(proc.get(), nullptr, 0, (LPTHREAD_START_ROUTINE)load_library, mem.get(), 0, nullptr);
		if (!thread) return false;

		WaitForSingleObject(thread.get(), INFINITE);

		return true;
	}

	static inline const char injectee_filename[] = "proxinject_injectee.dll";

	static HMODULE get_current_module()
	{
		HMODULE mod = nullptr;

		GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCTSTR)get_current_module, &mod);

		return mod;
	}

	static std::wstring get_current_filename() {
		wchar_t path[1024 + 1] = {};

		GetModuleFileNameW(get_current_module(), path, 1024);

		return path;
	}

	static std::optional<std::wstring> find_injectee(std::wstring_view self_binary_path) {
		auto path = fs::path(self_binary_path).replace_filename(injectee_filename);

		if (!std::filesystem::exists(path)) {
			return std::nullopt;
		}

		return path.wstring();
	}

	static bool inject(DWORD pid) {
		if (auto path = find_injectee(get_current_filename())) {
			return inject(pid, path.value());
		}

		return false;
	}
};
