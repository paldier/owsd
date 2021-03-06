cmake_minimum_required(VERSION 2.6)
project(owsd)
add_definitions(-Os -Wall -std=gnu11 -fms-extensions -fplan9-extensions -D_XOPEN_SOURCE=700 )

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules")

option(WSD_REQUIRE_SSL "Fail if SSL is not available" OFF)

option(WSD_HAVE_UBUS "ubus RPC support" OFF)
if (WSD_HAVE_UBUS)
	set(WSD_HAVE_UBUSPROXY ON CACHE BOOL "support for proxying remote ubus objects on local ubus")
else()
	unset(WSD_HAVE_UBUSPROXY CACHE)
endif()

option(WSD_HAVE_DBUS "DBus RPC support" OFF)
if (WSD_HAVE_DBUS)
	set(WSD_DBUS_OBJECTS_PATH "/se/inteno/iopsys" CACHE STRING
		"Path under which to (non-recursively) look for DBus objects")
else()
	unset(WSD_DBUS_OBJECTS_PATH CACHE)
endif()

if (NOT WSD_HAVE_UBUS AND NOT WSD_HAVE_DBUS)
	message(FATAL_ERROR "select at least one of DBus or ubus ...")
endif()

set(WSD_USER_BLACKLIST_OLD ON CACHE BOOL "Support old way of limiting user to specific listen interface")

set(SOURCES
	src/main.c
	src/config.c
	src/ws_http.c
	src/ws_http_serve.c
	src/wsubus.c
	src/rpc.c
	src/rpc_call.c
	src/rpc_list.c
	src/rpc_sub.c
	src/access_check.c
	src/util_jsonrpc.c
	src/ubusx_acl.c
	)

find_package(libwebsockets REQUIRED)
find_package(LIBUBOX REQUIRED)
find_library(JSON_LIBRARIES NAMES json-c)

set(WSD_LINK ${LIBWEBSOCKETS_LIBRARIES} ${LIBUBOX_LIBRARIES} ${JSON_LIBRARIES})
set(WSD_INCLUDE ${LIBWEBSOCKETS_INCLUDE_DIRS})

if (OWSD_JSON_VALIDATION)
	find_library(VALIDATOR_LIBRARIES NAMES json-validator)
	find_library(SCHEMA_VALIDATOR_LIBRARIES NAMES json-schema-validator)

	list(APPEND WSD_LINK ${VALIDATOR_LIBRARIES} ${SCHEMA_VALIDATOR_LIBRARIES})

	list(APPEND SOURCES
		src/util_json_validation.c
		)
endif()

if (WSD_HAVE_UBUS)
	find_library(UBUS_LIBRARIES NAMES ubus)
	find_path(UBUS_INCLUDE_DIRS libubus.h)
	list(APPEND WSD_LINK ${UBUS_LIBRARIES})
	list(APPEND WSD_INCLUDE ${UBUS_INCLUDE_DIRS})
	if (WSD_HAVE_UBUSPROXY)
		list(APPEND SOURCES
			src/local_stub.c
			src/ws_ubusproxy.c
			src/wsubus_client.c
			)
	endif()
	if (OWSD_UPROXYD)
		set(UPROXYD_LINK ${LIBUBOX_LIBRARIES} ${JSON_LIBRARIES} ${UBUS_LIBRARIES})
		set(UPROXYD_SOURCES
			src/uproxyd.c
			)
	endif()
endif()

if (WSD_HAVE_DBUS)
	find_package(DBus REQUIRED)
	list(APPEND WSD_LINK ${DBUS_LIBRARIES})
	list(APPEND WSD_INCLUDE ${DBUS_INCLUDE_DIRS})

	find_package(LibXml2 REQUIRED)
	list(APPEND WSD_LINK ${LIBXML2_LIBRARIES})
	list(APPEND WSD_INCLUDE ${LIBXML2_INCLUDE_DIR})

	list(APPEND SOURCES
		src/dbus-io.c
		src/util_dbus.c
		src/dubus_conversions_names.c
		src/dubus_conversions_types.c
		src/rpc_list_dbus.c
	)
endif()

configure_file(src/owsd-config.in.h owsd-config.h ESCAPE_QUOTES)
list(APPEND WSD_INCLUDE ${CMAKE_BINARY_DIR})

add_executable(owsd ${SOURCES})
target_link_libraries(owsd ${WSD_LINK})
if (OWSD_UPROXYD)
	add_executable(uproxyd ${UPROXYD_SOURCES})
	target_link_libraries(uproxyd ${UPROXYD_LINK})
endif()
include_directories(${WSD_INCLUDE})

#testing
IF(CMAKE_BUILD_TYPE STREQUAL Debug)
	OPTION(ENABLE_BUILD_TESTS "Build tests" ON)
	#OPTION(ENABLE_VALGRIND_TESTS "Build tests with valgrind" ON)
ELSE()
	OPTION(ENABLE_BUILD_TESTS "Build tests" OFF)
	#OPTION(ENABLE_VALGRIND_TESTS "Build tests with valgrind" OFF)
ENDIF()

IF(ENABLE_BUILD_TESTS)
	FIND_PACKAGE(CMocka)
	if(CMOCKA_FOUND)
		INCLUDE(CodeCoverage)
		APPEND_COVERAGE_COMPILER_FLAGS()
		SET(COVERAGE_EXCLUDES '*/main.c' '/usr/include/*' '*/*_test.c' '*/template.c' '*/uproxyd.c')
		ADD_LIBRARY(${PROJECT_NAME}-api SHARED ${SOURCES})
		MESSAGE("-- Building tests")
		ENABLE_TESTING()
		ADD_SUBDIRECTORY(test/cmocka)
		ADD_SUBDIRECTORY(test/cmocka/template_obj)
	ELSE(CMOCKA_FOUND)
		MESSAGE("-- CMocka not found")
	ENDIF(CMOCKA_FOUND)
ENDIF(ENABLE_BUILD_TESTS)

install(TARGETS owsd RUNTIME DESTINATION bin)
if (OWSD_UPROXYD)
	install(TARGETS uproxyd RUNTIME DESTINATION bin)
endif()
