# RayTracer, Scene and Utils include each other's headers cyclically (RayTracer needs
# Scene, Scene needs Utils, Utils needs Scene), so they are not separable static
# libraries. They form one component and are built as one.
#
# PixelBuffer lives under Window/ for historical reasons but is a plain float
# framebuffer with no windowing dependency, so it belongs to the core. Only Window.cpp
# actually needs GLFW/OpenGL, and that is the one file split into the viewer target.
add_library(pt_core STATIC
    RayTracer/Integrator.cpp
    RayTracer/RayTracer.cpp
    Scene/BVH.cpp
    Scene/Scene.cpp
    Scene/SceneObject.cpp
    Scene/Texture.cpp
    Utils/Config.cpp
    Utils/ImageIO.cpp
    Utils/ObjModel.cpp
    Utils/Stats.cpp
    Window/PixelBuffer.cpp
)

target_include_directories(pt_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pt_core
    PUBLIC nlohmann_json::nlohmann_json
    PRIVATE pt_warnings
)

if(PT_ENABLE_STATS)
    # PUBLIC: the counters are incremented from header-inlined traversal code, so every
    # consumer must agree on whether they exist.
    target_compile_definitions(pt_core PUBLIC PT_ENABLE_STATS)
endif()

add_executable(steradian main.cpp)
target_link_libraries(steradian PRIVATE pt_core pt_warnings)

if(PT_VIEWER_ENABLED)
    add_library(pt_viewer STATIC Window/Window.cpp)
    target_link_libraries(pt_viewer
        PUBLIC pt_core glfw OpenGL::GL
        PRIVATE pt_warnings
    )
    target_link_libraries(steradian PRIVATE pt_viewer)
    target_compile_definitions(steradian PRIVATE PT_HAVE_VIEWER)
endif()
