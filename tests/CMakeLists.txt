# Golden-image regression tests.
#
# Renders are deterministic for a fixed seed and sample count, so each test renders a
# small scene and compares it byte-for-byte (within a small tolerance) against a
# committed reference image. This is the safety net for the acceleration-structure and
# integrator work: those changes must not alter the image, and this is what proves it.

add_executable(image_compare image_compare.cpp)
target_link_libraries(image_compare PRIVATE pt_core pt_warnings)

# Checks the acceleration structure against a brute force scan. Golden images prove the
# final picture is unchanged; this proves the structure itself returns exactly the hits a
# linear search would, which is the property the previous octree quietly violated.
add_executable(bvh_consistency bvh_consistency.cpp)
target_link_libraries(bvh_consistency PRIVATE pt_core pt_warnings)

# Ray counts are per scene because the brute force reference, not the BVH, sets the cost:
# it is O(rays * primitives), so the 48,918 triangle scene gets proportionally fewer rays
# to keep the suite quick.
foreach(scene_and_rays "two_spheres:20000" "test_obj_ball:20000" "test_man_obj:2000")
    string(REPLACE ":" ";" parts ${scene_and_rays})
    list(GET parts 0 scene)
    list(GET parts 1 rays)

    add_test(NAME bvh.${scene}
        COMMAND bvh_consistency ${CMAKE_SOURCE_DIR}/res/scenes/${scene}.json ${rays})
    set_tests_properties(bvh.${scene} PROPERTIES TIMEOUT 300)
endforeach()

# Scenes kept deliberately small so the whole suite runs in a few seconds. Mesh scenes
# are slow until the BVH actually culls, so only the cheapest mesh is included for now.
set(GOLDEN_SCENES
    single_sphere
    single_triangle
    two_spheres
    test_obj_ball
    # 48,918 triangles. Only affordable as a test since the BVH started culling: this
    # scene took over a minute per render before, and now takes a fraction of a second.
    test_man_obj
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
