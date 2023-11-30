set(LIBRARY_NAME UTILS)

set(HEADERS Vec3.h Config.h ObjModel.h)
set(SOURCES Config.cpp ObjModel.cpp)

message(${CMAKE_SOURCE_DIR}/third_party/json/include)
message(${HEADERS} ${SOURCES})

add_library(${LIBRARY_NAME} STATIC ${HEADERS} ${SOURCES})
target_include_directories(${LIBRARY_NAME} PUBLIC ${CMAKE_SOURCE_DIR}/src/${LIBRARY_NAME} ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/third_party/json/include)
target_link_libraries(${CMAKE_PROJECT_NAME} PUBLIC ${LIBRARY_NAME} nlohmann_json::nlohmann_json)