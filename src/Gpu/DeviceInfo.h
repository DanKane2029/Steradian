#pragma once

/**
 * \brief Reporting on the GPU the renderer would use, and the toolchain behind it.
 *
 * This exists as a command rather than a throwaway probe because every one of the
 * numbers it prints is a decision input for the GPU backend, and because it is the
 * fastest way to tell a broken installation from a missing one. "OptiX did not
 * initialise" and "there is no device" want very different responses, and without this
 * they look identical from outside.
 */
namespace Gpu
{

/**
 * \brief Prints what the device and the OptiX implementation report about themselves.
 *
 * Writes to stdout. Diagnoses rather than throws: a machine with no usable device should
 * get an explanation, not a stack trace.
 *
 * \returns True when a device was found and OptiX initialised on it.
 */
auto printDeviceInfo() -> bool;

} // namespace Gpu
