# Golden-image regression tests.
#
# Renders are deterministic for a fixed seed and sample count, so each test renders a
# small scene and compares it byte-for-byte (within a small tolerance) against a
# committed reference image. This is the safety net for the acceleration-structure and
# integrator work: those changes must not alter the image, and this is what proves it.

add_executable(image_compare image_compare.cpp)
target_link_libraries(image_compare PRIVATE pt_core pt_warnings)

# Scenes kept deliberately small so the whole suite runs in a few seconds. Mesh scenes
# are slow until the BVH actually culls, so only the cheapest mesh is included for now.
set(GOLDEN_SCENES
    single_sphere
    single_triangle
    two_spheres
    test_obj_ball
)

set(GOLDEN_SAMPLES 4)
set(GOLDEN_SEED 1)

foreach(scene ${GOLDEN_SCENES})
    add_test(NAME golden.${scene}
        COMMAND ${CMAKE_COMMAND}
            -DRENDERER=$<TARGET_FILE:steradian>
            -DCOMPARE=$<TARGET_FILE:image_compare>
            -DCONFIG=${CMAKE_CURRENT_SOURCE_DIR}/test_config.json
            -DSCENE=${CMAKE_SOURCE_DIR}/res/scenes/${scene}.json
            -DGOLDEN=${CMAKE_CURRENT_SOURCE_DIR}/golden/${scene}.png
            -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/${scene}.png
            -DSAMPLES=${GOLDEN_SAMPLES}
            -DSEED=${GOLDEN_SEED}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/run_golden_test.cmake
    )
    set_tests_properties(golden.${scene} PROPERTIES TIMEOUT 300)
endforeach()

# Determinism is the property the golden tests rest on, so it is asserted directly:
# the same seed must produce the same bytes at different thread counts.
add_test(NAME determinism
    COMMAND ${CMAKE_COMMAND}
        -DRENDERER=$<TARGET_FILE:steradian>
        -DCOMPARE=$<TARGET_FILE:image_compare>
        -DCONFIG=${CMAKE_CURRENT_SOURCE_DIR}/test_config.json
        -DSCENE=${CMAKE_SOURCE_DIR}/res/scenes/two_spheres.json
        -DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/run_determinism_test.cmake
)
set_tests_properties(determinism PROPERTIES TIMEOUT 300)
