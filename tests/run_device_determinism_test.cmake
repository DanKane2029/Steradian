# Asserts that a GPU render depends only on (seed, samples).
#
# The CPU has an obvious knob to vary -- thread count -- and its determinism test turns
# it. The GPU has no equivalent: the launch shape follows the image. What it has instead
# is a great deal of state built fresh for every process, any of which could make a render
# depend on something other than its seed. Each run compiles the device code with NVRTC,
# creates a CUDA context, builds the acceleration structure, and allocates buffers that
# nothing has written yet. So this varies the thing that can be varied: it runs the render
# in separate processes and requires the bytes to match.
#
# The property matters more here than it looks. Every other GPU check leans on it. The
# furnace test measures one render and compares it to a physical constant; the
# cross-backend agreement test compares images from two seeds and reads the difference as
# noise. Both are meaningless if a render is not reproducible in the first place, and a
# regression there would show up in them as something else entirely -- a slightly wrong
# furnace number, a slightly raised noise ratio -- rather than as what it is.

foreach(var RENDERER COMPARE CONFIG OUTPUT_DIR)
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "run_device_determinism_test.cmake: ${var} was not set")
    endif()
endforeach()

set(runs 3)

# Two scenes, because they reach different device code. The first is spheres and a couple
# of triangles; the second is a refracting mesh over a procedural floor, which exercises
# the dielectric branch, absorption, and the parts of a path that bounce many times.
foreach(scene ${SCENES})
    set(reference "")

    foreach(run RANGE 1 ${runs})
        set(output "${OUTPUT_DIR}/device_determinism_${scene}_${run}.png")

        execute_process(
            COMMAND "${RENDERER}"
                    --config "${CONFIG}"
                    --scene "${SCENE_DIR}/${scene}.json"
                    --out "${output}"
                    --samples 32
                    --seed 1
                    --device gpu
            RESULT_VARIABLE render_result
            OUTPUT_VARIABLE render_output
            ERROR_VARIABLE render_output
        )

        if(NOT render_result EQUAL 0)
            message(FATAL_ERROR "GPU render of ${scene} failed:\n${render_output}")
        endif()

        if(reference STREQUAL "")
            set(reference "${output}")
        else()
            # Exact match required. Two runs of the same binary on the same device with
            # the same seed have no licence to differ at all, and accepting a tolerance
            # here would hide precisely the thing being looked for.
            execute_process(
                COMMAND "${COMPARE}" "${reference}" "${output}" 0 0
                RESULT_VARIABLE compare_result
                OUTPUT_VARIABLE compare_output
                ERROR_VARIABLE compare_output
            )

            if(NOT compare_result EQUAL 0)
                message(FATAL_ERROR
                    "The GPU render of ${scene} is not deterministic: runs 1 and ${run} "
                    "produced different images from the same seed.\n${compare_output}")
            endif()

            message(STATUS "${scene}: runs 1 and ${run} identical")
        endif()
    endforeach()
endforeach()
