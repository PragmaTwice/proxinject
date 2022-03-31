#include <memory>
#include <optional>
#include <Windows.h>

template <auto f>
struct static_function {
	template <typename T>
	decltype(auto) operator()(T&& x) const {
		return f(std::forward<T>(x));
	}
};

struct handle : std::unique_ptr<void, static_function<CloseHandle>> {
	using base_type = std::unique_ptr<void, static_function<CloseHandle>>;

	using base_type::base_type;

	handle(HANDLE hd) : base_type(hd) {}
};

struct virtual_memory {
	void* const proc_handle;
	void* const mem_addr;
	const size_t size_;

	virtual_memory(void* proc_handle, size_t size) : 
		proc_handle(proc_handle), 
		mem_addr(VirtualAllocEx(proc_handle, nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE)),
		size_(size) {}

	~virtual_memory() {
		if (mem_addr) {
			VirtualFreeEx(proc_handle, mem_addr, 0, MEM_RELEASE);
		}
	}

	operator bool() const {
		return mem_addr != nullptr;
	}

	void *get() const {
		return mem_addr;
	}

	void* process_handle() const {
		return proc_handle;
	}

	size_t size() const {
		return size_;
	}

	std::optional<size_t> write(const void *buf, size_t n) const {
		size_t written_size;
		
		if (WriteProcessMemory(proc_handle, mem_addr, buf, n, &written_size)) {
			return written_size;
		}
		
		return std::nullopt;
	}

	auto write(const void *buf) {
		return write(buf, size_);
	}

	std::optional<size_t> read(void *buf, size_t n) const {
		size_t read_size;

		if (ReadProcessMemory(proc_handle, mem_addr, buf, n, &read_size)) {
			return read_size;
		}

		return std::nullopt;
	}

	auto read(void* buf) {
		return read(buf, size_);
	}
};
