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
    Scene/Geometry.cpp
    Scene/Scene.cpp
    Scene/Texture.cpp
    Utils/Config.cpp
    Utils/Denoiser.cpp
    Utils/ImageIO.cpp
    Utils/Microfacet.cpp
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

if(PT_GPU_ENABLED)
    # Kept out of pt_core deliberately. The renderer core, and therefore every test,
    # must not acquire a dependency on the GPU SDK; only the executable links this.
    add_library(pt_gpu STATIC Gpu/DeviceInfo.cpp)

    target_include_directories(pt_gpu PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${PT_GPU_SDK_DIR}/include)

    # libcuda is linked through the stub rather than the installed driver. The stub
    # exports the same entry points under the same soname, so the loader binds to the
    # real driver at run time -- which is also what makes it possible to build this on a
    # machine that has the headers but no NVIDIA driver at all.
    target_link_libraries(pt_gpu
        PUBLIC ${PT_GPU_SDK_DIR}/lib/stubs/libcuda.so
               ${PT_GPU_SDK_DIR}/lib/libnvrtc.so
               ${CMAKE_DL_LIBS}
        PRIVATE pt_warnings
    )

    # NVRTC is not installed system-wide, so the executable has to be told where the copy
    # fetched by setup-gpu-deps.sh lives. OptiX needs no such entry: optix_stubs.h opens
    # libnvoptix.so.1 by name at run time, and the driver put that on the default path.
    set_target_properties(steradian PROPERTIES BUILD_RPATH "${PT_GPU_SDK_DIR}/lib")

    target_link_libraries(steradian PRIVATE pt_gpu)
    target_compile_definitions(steradian PRIVATE PT_HAVE_GPU)
endif()

if(PT_VIEWER_ENABLED)
    add_library(pt_viewer STATIC
        Window/CameraController.cpp
        Window/Window.cpp
    )
    target_link_libraries(pt_viewer
        PUBLIC pt_core glfw OpenGL::GL
        PRIVATE pt_warnings
    )
    target_link_libraries(steradian PRIVATE pt_viewer)
    target_compile_definitions(steradian PRIVATE PT_HAVE_VIEWER)
endif()
