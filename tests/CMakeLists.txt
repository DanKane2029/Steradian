# Golden-image regression tests.
#
# Renders are deterministic for a fixed seed and sample count, so each test renders a
# small scene and compares it byte-for-byte (within a small tolerance) against a
# committed reference image. This is the safety net for the acceleration-structure and
# integrator work: those changes must not alter the image, and this is what proves it.

# Placement maths, and above all the normal transform. A normal scaled like a point stops
# being perpendicular to its surface, which produces wrong shading that looks entirely
# plausible in an image, so it needs checking numerically rather than by eye.
add_executable(transform_test transform_test.cpp)
target_link_libraries(transform_test PRIVATE pt_core pt_warnings)

add_test(NAME transform COMMAND transform_test)
set_tests_properties(transform PROPERTIES TIMEOUT 60)

add_executable(image_compare image_compare.cpp)
target_link_libraries(image_compare PRIVATE pt_core pt_warnings)

# Checks the acceleration structure against a brute force scan. Golden images prove the
# final picture is unchanged; this proves the structure itself returns exactly the hits a
# linear search would, which is the property the previous octree quietly violated.
# White furnace test: a fully reflective surface in a uniform environment must render as
# exactly that environment. Almost any error in the path loop -- a dropped cosine, a
# density applied the wrong way round, a bounce that loses throughput -- shows up here.
add_executable(furnace furnace.cpp)
target_link_libraries(furnace PRIVATE pt_core pt_warnings)

add_test(NAME furnace COMMAND furnace ${CMAKE_CURRENT_SOURCE_DIR}/furnace_scene.json 20000)
set_tests_properties(furnace PROPERTIES TIMEOUT 300)

# The same check for a rough conductor. Single-scattering GGX loses energy as roughness
# rises, because light bouncing more than once between microfacets is not modelled, so
# this uses a low roughness where that loss is still under a percent. It covers the
# microfacet sampling weight, which the diffuse furnace never touches.
add_test(NAME furnace.metal COMMAND furnace ${CMAKE_CURRENT_SOURCE_DIR}/furnace_metal_scene.json 20000)
set_tests_properties(furnace.metal PROPERTIES TIMEOUT 300)

add_executable(bvh_consistency bvh_consistency.cpp)
target_link_libraries(bvh_consistency PRIVATE pt_core pt_warnings)

# Ray counts are per scene because the brute force reference, not the BVH, sets the cost:
# it is O(rays * primitives), so the 48,918 triangle scene gets proportionally fewer rays
# to keep the suite quick.
foreach(scene_and_rays "two_spheres:20000" "test_obj_ball:20000" "test_man_obj:2000" "bunny:800")
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
    cornell_box
    glossy_spheres
    # A glass sphere in front of red, green and blue spheres. Through it the order is
    # reversed, which is what a lens does and what nothing else in the scene can fake, so
    # the reference image pins refraction down rather than merely exercising it.
    glass_lens
    # Clear, tinted and deeply absorbing glass side by side. Absorption depends on the
    # distance light travels inside the material, so each sphere is lighter at its rim
    # than through its middle, which a surface tint cannot reproduce.
    glass_tinted
    # The Stanford models. The dragon at 202,520 triangles is by a wide margin the
    # heaviest thing the acceleration structure is asked to organize, which is the point
    # of including it.
    bunny
    dragon
)

set(GOLDEN_SAMPLES 64)
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
