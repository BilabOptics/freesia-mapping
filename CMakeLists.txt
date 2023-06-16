cmake_minimum_required(VERSION 3.21)

project(Freesia VERSION 2.1.0)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 COMPONENTS Core Gui Widgets Network LinguistTools REQUIRED)
find_package(VTK REQUIRED)
find_package(OpenCV REQUIRED)

set(tgt ${PROJECT_NAME}-${PROJECT_VERSION})

set(SRC_DIR ${CMAKE_CURRENT_LIST_DIR}/freesia)
file(
	GLOB_RECURSE Freesia_SRCS LIST_DIRECTORIES false
	${SRC_DIR}/*.cpp ${SRC_DIR}/*.h
)

qt5_add_resources(Freesia_SRCS ${SRC_DIR}/assets.qrc)

add_executable(${tgt} ${Freesia_SRCS} ${SRC_DIR}/app.rc)

target_link_libraries(
	${tgt} PRIVATE
	Qt5::Core Qt5::Gui Qt5::Widgets Qt5::Network
	${VTK_LIBRARIES} ${OpenCV_LIBS} ${ZLIB_LIBRARIES}
	${NN_DIR}/x64/Release/nn.lib
)
target_include_directories(${tgt} PRIVATE ${NN_DIR}/nn)

if (MSVC)
  #  Comment this line to show the console window
  set_property(TARGET ${tgt} PROPERTY WIN32_EXECUTABLE true)
elseif (APPLE)
else ()
  set(CMAKE_CXX_FLAGS "-no-pie")
  set(CMAKE_C_FLAGS "-no-pie")
endif ()
