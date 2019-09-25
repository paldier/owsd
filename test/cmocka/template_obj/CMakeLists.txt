cmake_minimum_required(VERSION 2.6)
project(template_obj C)
add_definitions(-Os -Wall -std=gnu11 -fms-extensions -fplan9-extensions -D_XOPEN_SOURCE=700 )

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules")

set(SOURCES
	template.c
	)

find_package(LIBUBOX REQUIRED)
find_library(JSON_LIBRARIES NAMES json-c)

set(TEMPLATE_LINK ${LIBUBOX_LIBRARIES} ${JSON_LIBRARIES})

find_library(UBUS_LIBRARIES NAMES ubus)
find_path(UBUS_INCLUDE_DIRS libubus.h)
list(APPEND TEMPLATE_LINK ${UBUS_LIBRARIES})
list(APPEND TEMPLATE_INCLUDE ${UBUS_INCLUDE_DIRS})

add_executable(template ${SOURCES})
target_link_libraries(template ${WSD_LINK})

install(TARGETS template RUNTIME DESTINATION bin)
