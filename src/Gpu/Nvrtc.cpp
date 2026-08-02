#include "Nvrtc.h"

#include <cstdio>

#include <dlfcn.h>

namespace
{

/**
 * \brief The builtins library, named as NVRTC itself will ask for it.
 *
 * Versioned deliberately. NVRTC opens this exact soname, so an unversioned name would
 * load something and still leave NVRTC's own request unsatisfied.
 */
constexpr const char *builtinsName = "libnvrtc-builtins.so.12.0";

} // namespace

auto Gpu::prepareNvrtc() -> bool
{
    static const bool ready = [] {
        // Already present, either because the system has a CUDA installation or because
        // something set LD_LIBRARY_PATH. Nothing to do.
        if (void *existing = dlopen(builtinsName, RTLD_NOLOAD | RTLD_LAZY); existing != nullptr)
        {
            return true;
        }

        // Otherwise take it from where setup-gpu-deps.sh put it. RTLD_GLOBAL is the part
        // that matters: it publishes the library under its soname so NVRTC's own dlopen,
        // which searches by that name, finds it already loaded.
        const char *path = PT_GPU_SDK_LIB_DIR "/"
                                              "libnvrtc-builtins.so.12.0";

        if (dlopen(path, RTLD_NOW | RTLD_GLOBAL) != nullptr)
        {
            return true;
        }

        std::fprintf(stderr,
                     "warning: could not load %s.\n"
                     "         NVRTC will fail to compile device code. Re-run "
                     "scripts/setup-gpu-deps.sh.\n",
                     path);
        return false;
    }();

    return ready;
}
