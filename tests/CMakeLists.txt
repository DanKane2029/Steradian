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

# The procedural checker, and specifically its behaviour on a plane that lies exactly on a
# cell boundary. That is where scenes put their floors, and getting it wrong speckles the
# floor instead of failing outright, so it needs asserting rather than looking at.
add_executable(checker_test checker_test.cpp)
target_link_libraries(checker_test PRIVATE pt_core pt_warnings)

add_test(NAME checker COMMAND checker_test)
set_tests_properties(checker PROPERTIES TIMEOUT 60)

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

# The same check for a rough conductor, at a roughness where it means something. A single
# bounce off the surface loses a great deal of light at 0.8, returning about 56% of it, so
# this fails outright without the compensation that puts the rest back. It also covers the
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
    # Two spheres sharing one image texture, tinted differently by their own albedos.
    # Textures had no coverage at all until this scene: nothing else in res/scenes
    # references an image, so every change to texture loading, sampling or lookup was
    # unguarded. Sharing one file between two materials is deliberate -- the scene loads
    # each image once and refers to it by index, and that sharing is what this pins.
    textured_spheres
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
    # The dragon again, in glass, over a checkered floor. This is the only scene where a
    # procedural material and a refracting mesh meet, which is the combination worth
    # guarding: the checker is evaluated at whatever position a path arrives at, so it
    # covers the floor, the floor seen through the glass, and the floor seen in its
    # reflections, all from one pattern with a shape that is obvious when it goes wrong.
    #
    # Note this renders darker than the same scene does normally. The shared test config
    # caps paths at three bounces to keep the suite quick, and glass spends bounces faster
    # than anything else, so most paths through the body are cut off before reaching a
    # light. That costs the reference nothing as a regression test, but it is not what the
    # scene is supposed to look like -- see docs/glass_dragon.png for that.
    glass_dragon
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

# Cross-backend agreement for the shared maths. Only meaningful with a GPU, so it is built
# only when GPU support is, and it is the check that Stage 2's whole claim rests on: that
# Vec3, Rng, Sampling and Microfacet compile unchanged as device code and compute the same
# things there.
#
# It reads the headers straight out of the source tree and hands them to NVRTC, so a
# header that stops being device-portable fails here rather than months later.
if(PT_GPU_ENABLED)
    add_executable(device_parity device_parity.cpp)

    target_link_libraries(device_parity PRIVATE pt_core pt_gpu pt_warnings)
    target_compile_definitions(device_parity PRIVATE PT_SOURCE_DIR="${CMAKE_SOURCE_DIR}/src")
    set_target_properties(device_parity PROPERTIES BUILD_RPATH "${PT_GPU_SDK_DIR}/lib")

    add_test(NAME device.parity COMMAND device_parity 65536)
    set_tests_properties(device.parity PROPERTIES TIMEOUT 300)
endif()
