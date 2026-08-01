# Asserts that a render depends only on (seed, samples) and not on thread count.
#
# This is the property every golden test rests on. It also guards against a whole class
# of threading regression: if a future change introduces a race in the framebuffer or in
# seeding, the same seed will stop producing the same image and this test fails.

foreach(var RENDERER COMPARE CONFIG SCENE OUTPUT_DIR)
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "run_determinism_test.cmake: ${var} was not set")
    endif()
endforeach()

set(thread_counts 1 3 8)

# Checked with adaptive sampling on as well as off. Adaptive is where this property is
# most easily lost: how many samples a pixel takes is decided while rendering, so any
# dependence on how the image was divided between threads would show up here as a
# different image rather than as an obvious failure.
foreach(mode "fixed" "adaptive")
set(reference "")

if(mode STREQUAL "adaptive")
    set(extra_args --adaptive 0.02)
else()
    set(extra_args)
endif()

foreach(threads ${thread_counts})
    set(output "${OUTPUT_DIR}/determinism_${mode}_t${threads}.png")

    execute_process(
        COMMAND "${RENDERER}"
                --config "${CONFIG}"
                --scene "${SCENE}"
                --out "${output}"
                --samples 32
                --seed 1
                --threads ${threads}
                ${extra_args}
        RESULT_VARIABLE render_result
        OUTPUT_VARIABLE render_output
        ERROR_VARIABLE render_output
    )

    if(NOT render_result EQUAL 0)
        message(FATAL_ERROR "Render with ${threads} thread(s), ${mode}, failed:\n${render_output}")
    endif()

    if(reference STREQUAL "")
        set(reference "${output}")
    else()
        # Exact match required: same binary, same machine, same seed.
        execute_process(
            COMMAND "${COMPARE}" "${reference}" "${output}" 0 0
            RESULT_VARIABLE compare_result
            OUTPUT_VARIABLE compare_output
            ERROR_VARIABLE compare_output
        )

        if(NOT compare_result EQUAL 0)
            message(FATAL_ERROR
                "Render is not deterministic with ${mode} sampling: 1 thread and "
                "${threads} threads produced different images.\n${compare_output}")
        endif()

        message(STATUS "${mode}: 1 vs ${threads} threads identical")
    endif()
endforeach()
endforeach()
