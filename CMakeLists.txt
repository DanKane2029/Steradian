cmake_minimum_required(VERSION 3.0)

project(path_tracer)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

# Set Project Directories
set(SRC_DIR src)
set(WINDOW_DIR ${SRC_DIR}/Window)
set(UTILS_DIR ${SRC_DIR}/Utils)
set(SCENE_DIR ${SRC_DIR}/Scene)
set(RAY_TRACER_DIR ${SRC_DIR}/RayTracer)

# Util Classes
set(VEC3 ${UTILS_DIR}/Vec3.h)
set(MODEL_LOADER ${UTILS_DIR}/ModelLoader.h ${UTILS_DIR}/ModelLoader.cpp)
set(UTILS ${VEC3} ${MODEL_LOADER})

# Window Classes
set(WINDOW ${WINDOW_DIR}/Window.h ${WINDOW_DIR}/Window.cpp)
set(PIXEL_BUFFER ${WINDOW_DIR}/PixelBuffer.h ${WINDOW_DIR}/PixelBuffer.cpp)
set(WINDOW_TOOLS ${WINDOW} ${PIXEL_BUFFER})

# Scene Classes
set(SCENE ${SCENE_DIR}/Scene.h ${SCENE_DIR}/Scene.cpp)
set(SCENE_OBJECT ${SCENE_DIR}/SceneObject.h ${SCENE_DIR}/SceneObject.cpp)
set(LIGHT ${SCENE_DIR}/Light.h ${SCENE_DIR}/Light.cpp)
set(MATERIAL ${SCENE_DIR}/Material.h)
set(BOUNDING_BOX ${SCENE_DIR}/BoundingBox.h)
set(BVH ${SCENE_DIR}/BVH.h ${SCENE_DIR}/BVH.cpp)
set(SCENE_TOOLS ${SCENE} ${SCENE_OBJECT} ${LIGHT} ${MATERIAL} ${BOUNDING_BOX} ${BVH})

# Ray Tracer Classes
set(RAY ${RAY_TRACER_DIR}/Ray.h)
set(HIT ${RAY_TRACER_DIR}/Hit.h)
set(CAMERA ${RAY_TRACER_DIR}/Camera.h)
set(RAY_TRACER ${RAY_TRACER_DIR}/RayTracer.h ${RAY_TRACER_DIR}/RayTracer.cpp)
set(RAY_TRACER_TOOLS ${RAY} ${HIT} ${CAMERA} ${RAY_TRACER})

# All Sources
set(SOURCES ${SRC_DIR}/Main.cpp ${UTILS} ${WINDOW_TOOLS} ${SCENE_TOOLS} ${RAY_TRACER_TOOLS})

add_executable(path_tracer ${SOURCES})
target_include_directories(path_tracer PUBLIC ${UTILS_DIR} ${WINDOW_DIR} ${SCENE_DIR} ${RAY_TRACER_DIR})
target_link_libraries(path_tracer ${CONAN_LIBS})
target_compile_definitions(path_tracer PUBLIC CMAKE_SOURCE_DIR)
