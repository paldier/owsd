CMAKE_MINIMUM_REQUIRED(VERSION 3.0)
INCLUDE_DIRECTORIES ("${PROJECT_SOURCE_DIR}/src")
INCLUDE_DIRECTORIES ("${PROJECT_SOURCE_DIR}")
FILE(COPY files DESTINATION .)

find_library(UBUS_LIBRARIES NAMES ubus)
find_library(JSON_LIBRARIES NAMES json-c)
find_library(CURL_LIBRARIES NAMES curl)
find_library(EDITOR_LIBRARIES NAMES json-editor)

SET(functional_tests ubusx_test rpc_test http_test ubus_test)
SET(unit_tests owsd_test)
SET(all_tests owsd_test ubusx_test rpc_test http_test ubus_test)
FOREACH(test_name IN LISTS all_tests)
	ADD_EXECUTABLE(${test_name} ${test_name}.c)
	TARGET_LINK_LIBRARIES(
		${test_name}
		${CMOCKA_LIBRARIES}
		owsd-api
		${LIBUBOX_LIBRARIES}
		${LIBUCI_LIBRARIES}
		${UBUS_LIBRARIES}
		${JSON_LIBRARIES}
		${LIBWEBSOCKETS_LIBRARIES}
		${VALIDATOR_LIBRARIES}
		${SCHEMA_VALIDATOR_LIBRARIES}
		${CURL_LIBRARIES}
		${EDITOR_LIBRARIES}
		pthread
	)
	#ADD_TEST(NAME ${test_name} COMMAND $<TARGET_FILE:${test_name}>)
ENDFOREACH(test_name)

ADD_CUSTOM_TARGET(functional-test)
FOREACH(test_name IN LISTS functional_tests)
	ADD_CUSTOM_COMMAND(TARGET functional-test
		COMMAND ./${test_name}
	)
	ADD_TEST(NAME ${test_name} COMMAND $<TARGET_FILE:${test_name}>)
	SET_TESTS_PROPERTIES(${test_name} PROPERTIES LABELS "functional")
ENDFOREACH(test_name)

ADD_CUSTOM_TARGET(unit-test)
FOREACH(test_name IN LISTS unit_tests)
	ADD_CUSTOM_COMMAND(TARGET unit-test
		COMMAND ./${test_name}
	)
	ADD_TEST(NAME ${test_name} COMMAND $<TARGET_FILE:${test_name}>)
	SET_TESTS_PROPERTIES(${test_name} PROPERTIES LABELS "unit")
ENDFOREACH(test_name)

SETUP_TARGET_FOR_COVERAGE(
	NAME functional-coverage
	EXECUTABLE owsd -f /builds/iopsys/owsd/test/cmocka/files/tmp/owsd/cfg.json > /dev/null 2>&1
	DEPENDENCIES owsd-api
)

SETUP_TARGET_FOR_COVERAGE(
	NAME unit-coverage
	EXECUTABLE ctest -L unit
	DEPENDENCIES owsd-api
)

SETUP_TARGET_FOR_COVERAGE(
	NAME functional-api-coverage
	EXECUTABLE owsd -f /builds/iopsys/owsd/test/cmocka/files/tmp/owsd/cfg.json > /dev/null 2>&1
	DEPENDENCIES owsd-api
)
