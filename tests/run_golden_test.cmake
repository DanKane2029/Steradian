# Renders a scene headlessly and compares the result against a committed reference.
#
# Invoked by CTest via `cmake -P`. Expects RENDERER, COMPARE, CONFIG, SCENE, GOLDEN,
# OUTPUT, SAMPLES and SEED to be passed with -D.

foreach(var RENDERER COMPARE CONFIG SCENE GOLDEN OUTPUT SAMPLES SEED)
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "run_golden_test.cmake: ${var} was not set")
    endif()
endforeach()

if(NOT EXISTS "${GOLDEN}")
    message(FATAL_ERROR
        "Missing reference image: ${GOLDEN}\n"
        "Generate the references once with: tests/update_golden.sh <build-dir>")
endif()

execute_process(
    COMMAND "${RENDERER}"
            --config "${CONFIG}"
            --scene "${SCENE}"
            --out "${OUTPUT}"
            --samples "${SAMPLES}"
            --seed "${SEED}"
            --threads 4
    RESULT_VARIABLE render_result
    OUTPUT_VARIABLE render_output
    ERROR_VARIABLE render_output
)

if(NOT render_result EQUAL 0)
    message(FATAL_ERROR "Render failed (${render_result}):\n${render_output}")
endif()

execute_process(
    COMMAND "${COMPARE}" "${GOLDEN}" "${OUTPUT}"
    RESULT_VARIABLE compare_result
    OUTPUT_VARIABLE compare_output
    ERROR_VARIABLE compare_output
)

message(STATUS "${compare_output}")

if(NOT compare_result EQUAL 0)
    message(FATAL_ERROR
        "Output does not match the reference.\n"
        "  reference: ${GOLDEN}\n"
        "  actual:    ${OUTPUT}\n"
        "If this change is intentional, refresh the references with "
        "tests/update_golden.sh and review the image diff in the commit.")
endif()
