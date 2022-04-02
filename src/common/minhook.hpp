#include <minhook.h>

struct minhook {
  struct status {
    MH_STATUS v;

    status(MH_STATUS v) : v(v) {}

    operator MH_STATUS() const { return v; }
    bool ok() const { return v == MH_OK; }
    bool error() const { return v != MH_OK; }
  };

  static status init() { return MH_Initialize(); }

  static status deinit() { return MH_Uninitialize(); }

  template <typename F> static status enable(F *api) {
    return MH_EnableHook(reinterpret_cast<LPVOID>(api));
  }

  template <typename F> static status disable(F *api) {
    return MH_DisableHook(reinterpret_cast<LPVOID>(api));
  }

  static constexpr inline void *all_hooks = MH_ALL_HOOKS;

  static status enable() { return enable(all_hooks); }

  static status disable() { return disable(all_hooks); }

  template <typename F>
  static status create(F *target, F *detour, F *&original) {
    return MH_CreateHook(reinterpret_cast<LPVOID>(target),
                         reinterpret_cast<LPVOID>(detour),
                         reinterpret_cast<LPVOID *>(&original));
  }

  template <typename F> static status remove(F *target) {
    return MH_RemoveHook(reinterpret_cast<LPVOID>(target));
  }

  template <auto syscall, typename derived> struct api {
    using F = decltype(syscall);

    static inline F original = nullptr;

    static status create() {
      return minhook::create(syscall, derived::detour, original);
    }

    static status remove() { return minhook::remove(syscall); }
  };
};
