#pragma once

namespace Gpu
{

/**
 * \brief Makes NVRTC's builtins library findable before NVRTC looks for it.
 *
 * NVRTC cannot compile anything without libnvrtc-builtins, which it opens by name at the
 * moment of the first compilation rather than at load. That search does not consult this
 * executable's runpath -- the search is performed by libnvrtc, and uses libnvrtc's own,
 * which is empty -- so a copy sitting beside libnvrtc in a fetched SDK is not found, and
 * the failure arrives as "failed to open libnvrtc-builtins.so" from inside a compile that
 * looked like it should have worked.
 *
 * Opening it here first, from the path the SDK was fetched to, puts it in the process
 * under its own soname. NVRTC's later request then resolves to the already-loaded copy.
 * The alternative is requiring LD_LIBRARY_PATH to be set around every invocation, which
 * is not something a renderer should ask of whoever runs it.
 *
 * Safe to call repeatedly; the work happens once.
 *
 * \returns True if the builtins are available, either because this loaded them or
 *          because the system already had them.
 */
auto prepareNvrtc() -> bool;

} // namespace Gpu
