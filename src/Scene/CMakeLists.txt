set(LIBRARY_NAME SCENE)

set(HEADERS BVH.h Light.h Material.h Scene.h SceneObject.h)
set(SOURCES BVH.cpp Light.cpp Scene.cpp SceneObject.cpp)

add_library(${LIBRARY_NAME} STATIC ${HEADERS} ${SOURCES})
target_include_directories(${LIBRARY_NAME} PUBLIC ${CMAKE_SOURCE_DIR}/src/${LIBRARY_NAME} ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/third_party/json/include)
target_link_libraries(${CMAKE_PROJECT_NAME} PUBLIC ${LIBRARY_NAME} nlohmann_json::nlohmann_json)