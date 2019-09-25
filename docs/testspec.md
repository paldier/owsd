# Test Specification

The OWSD tests are written according to its feature requirements, defined in the
[function specification](./functionspec.md#requirements) and is tested through
functional tests. Due OWSD being an implementation of libwebsockets, and
libwebsockets keeping many of its structures private, only allowing limited
access through its API, it is necessary that all of OWSD tests are done in a
functional manner, having an active instance of owsd running in the environment,
stressing the code through libraries which opens communication to OWSD, such as
libwebsockets (RPC tests), libubus (ubus-x and ubus tests) and libcurl
(HTTP tests).

No unit tests are performed, and while API testing is prepared, the
functionality exposed over the ubus API is very limited, limiting the value of
the API tests.

## Prerequisites

Apart from the required library dependencies specified in the
[README](../README.md#dependencies), four process have to be running in the
test environment:
- `ubusd` - Allowing ubus objects to be published.
- `template_obj` - Object developed for testing purposes, publishing dummy
methods, events and other features useful for testing.
- `rpcd` - For session management.
- `owsd` - The binaries under test. For the ubus-x tests, two instances should
be active, running with a server, and a client configuration.

Additionally, configuration files, access lists and schemas should be
installed in the environment. Some prerequisites files and binaries come
installed on the IOPSYS gitlab-ci docker, others can be found under the test
subdirectory and are prepared by the gitlab-ci pipeline test scripts.

### Functional Tests

The OWSD functional tests are written in Cmocka, and consists of four different
testing suites, an http, rpc, ubus and ubusx test suite. These tests are aimed
at verifying the functionality of owsd at large, rather than testing individual
sections of code, none of these tests invoke an actual owsd function
through the compiled shared library of the source code, but rather stress the
owsd functionality through libubus, libcurl, and libwebsockets.

#### RPC Tests

The RPC tests are all implemented through libwebsockets, using the JSON-RPC
format to form requests.

| Serial ID		| Test Case Name	                                                            |
| :---			| :---				                                                            |
| 1				| [test_rpc_login](#test_rpc_login)	                                            |
| 2				| [test_rpc_method_call](#test_rpc_method_call)	                                |
| 3				| [test_rpc_method_call_denied](#test_rpc_method_call_denied)	                |
| 4				| [test_rpc_subscribe](#test_rpc_subscribe)	                                    |
| 5				| [test_rpc_subscribe_denied](#test_rpc_subscribe_denied)	                    |
| 6				| [test_rpc_valid_input_format](#test_rpc_valid_input_format)	                |
| 7				| [test_rpc_invalid_input_format](#test_rpc_invalid_input_format)	            |
| 8				| [test_rpc_valid_output_format](#test_rpc_valid_output_format)	                |
| 9				| [test_rpc_invalid_output_format](#test_rpc_invalid_output_format)	            |
| 10			| [test_rpc_parse_error](#test_rpc_parse_error)	                                |
| 11			| [test_rpc_subscribe_list](#test_rpc_subscribe_list)	                        |

##### Prerequisites

For the RPC tests, it is necessary that a user is created in the environment,
through i.e.

```
$ adduser user
```

The created user, should also be made available to use for login through RPCD,
by adding it to the configuration file, `/etc/config/rpcd`.

```
config login
	option username 'user'
	option password '$p$user'
	list read 'user'
	list write 'unauthenticated'
```

Lastly, schemas and access lists have to be made available in the environment,
these files may be found at `./test/cmocka/files/usr/share/acl.d/` and
`./test/cmocka/files/usr/share/schemas/`, and should be placed accordingly
in the environment, barring `./test/cmocka/files` from the path.

##### test_rpc_login

###### Description

Try to login with the user `user`, verify that a session is created.

###### Test Steps

Use the default session (`00000000000000000000000000000000`) to attempt to login
through the `session->login` method, with the arguments `{"username":"user", "password":"user"}`.

###### Test Expected Results

The return code 0 (`UBUS_STATUS_OK`) and a valid, non-null, SID
(`ubus_rpc_session`).

##### test_rpc_method_call

###### Description

Issue a method call to the test method,`restricted_template->test` through
the websocket using a `user` session, verify that it does not cause an error.

###### Test Steps

Use the SID form the previous test to invoke the `restricted_template->test`
method, to which access is granted by `/usr/share/rpcd/acl.d/user.json`. This
uses the RPC `call` method.

###### Test Expected Results

The return code 0, (`UBUS_STATUS_OK`).

##### test_rpc_method_call_denied

###### Description

Issue a method call to the test method,`template->test` through the websocket
using a `user` session.

###### Test Steps

Use the SID form the initial test to invoke the `template->test` method, to
which access is NOT granted by `/usr/share/rpcd/acl.d/user.json`.

###### Test Expected Results

The call should not go through and the return code 6
(`UBUS_STATUS_PERMISSION_DENIED`) is expected.

##### test_rpc_subscribe

###### Description

Attempt to subscribe to ubus events of the `restricted_template` type to verify
that such events are observed.

###### Test Steps

Use the RPC method `subscribe` to `restricted_template` events, allowed by
the `user.json` access list. Poll the websocket for four seconds to attempt to
record such an event.

###### Test Expected Results

An event of type `restricted_template` should be recorded in the test
environment context.

##### test_rpc_subscribe_denied

###### Description

Attempt to subscribe to ubus events of the `template` type to verify that no
such events are observed.

###### Test Steps

Use the RPC method `subscribe` to `template` events, to which access is denied
by the `user.json` access list. Poll the websocket for four seconds to attempt
to record such an event.

###### Test Expected Results

No event of type `template` to be recorded (the test environment context `type`
should be unset).

##### test_rpc_valid_input_format

###### Description

Attempt to call a method to which access is allowed for the `user` session, to
verify that it does not trigger a input or output validation failure.

###### Test Steps

Use the RPC call method to invoke the method `restricted_template->test`.

###### Test Expected Results

The test environment context to record the return code 0 (`UBUS_STATUS_OK`).

##### test_rpc_invalid_input_format

###### Description

Attempt to call a method to which access is allowed for the `user` session,
with invalid input parameters (input that does not adhere to the schema) to
verify that it does trigger a validation failure.

###### Test Steps

Use the RPC call method to access the method `invalid_template->test`, a method
which its schema states it explicitly requires an argument, of type integer.

###### Test Expected Results

The test environment context to record the return code -32605
(`JSONRPC_ERRORCODE__OUTPUT_FORMAT_ERROR`).

##### test_rpc_valid_output_format

###### Description

Attempt to call a method to which access is allowed for the `user` session,
and has an output and a schema, to validate that it does not trigger
a validation failure.

###### Test Steps

Use the RPC call method to invoke the method `restricted_template->test`.

###### Test Expected Results

The test environment context to record the return code 0 (`UBUS_STATUS_OK`).

##### test_rpc_invalid_output_format

###### Description

Attempt to call a method to which access is allowed for the `user` session,
which has an output that does not adhere to its schema file. Verify that the
correct error code is returned.

###### Test Steps

Use the RPC call method to access the method `invalid_template->test`.

###### Test Expected Results

The test environment context to record the return code -32605
(`JSONRPC_ERRORCODE__OUTPUT_FORMAT_ERROR`).

##### test_rpc_parse_error

###### Description

Attempt to make an RPC request with an incomplete JSON-RPC to verify the error
handling of websockets.

###### Test Steps

Prepare an JSON-RPC which is not complete JSON.

###### Test Expected Results

The test environment context to record the return code -32700
(`JSONRPC_ERRORCODE__PARSE_ERROR`).

##### test_rpc_subscribe_list

###### Description

Make an RPC which uses the subscribe-list functionality, to verify that the
functionality is implemented.

###### Test Steps

Subscribe to a `test` event, then prepare and issue an JSON-RPC which uses the
subscribe-list method.

###### Test Expected Results

Verify that the response has the expected SID subscribed to `test` events.

#### Ubus Proxy Tests

The ubus-x tests are performed using the libubus API to invoke methods and
register listeners.

| Serial ID		| Test Case Name	                                                                |
| :---			| :---				                                                                |
| 1				| [test_proxied_objects](#test_proxied_objects)	                                    |
| 2				| [test_proxied_access_ubusx_acl_success](#test_proxied_access_ubusx_acl_success)	|
| 3				| [test_proxied_access_ubusx_acl_fail](#test_proxied_access_ubusx_acl_fail)	        |
| 4				| [test_proxied_events_acl_fail](#test_proxied_events_acl_fail)	                    |
| 5				| [test_proxied_events_acl_success](#test_proxied_events_acl_success)	            |
| 6				| [test_call_toggling_object](#test_call_toggling_object)	                        |
| 7				| [test_observe_toggling_add](#test_observe_toggling_add)	                        |
| 8				| [test_observe_toggling_remove](#test_observe_toggling_remove)	                    |
| 9				| [test_ubusx_acl_init](#test_ubusx_acl_init)	                                    |

##### Prerequisites

The first requirement for the ubus-x tests is to have valid SSL certificates
in the system. In the provided owsd configuration files at
`./test/cmocka/files/tmp/owsd/` the path which is searched for the certificates
is defined as `/etc/ubusx/` and can in the test subdirectory be found at
`./test/cmocka/files/etc/ubusx/`.

For the ubus-x tests, it is necessary that two instances of OWSD are running in
the same test environment. One with a _server_ configuration
(`./test/cmocka/files/tmp/owsd/cfg.json`), and one with a _client_
(`./test/cmocka/files/tmp/owsd/rpt.json`) configuraiton, this way, owsd can
connect to the other instance of itself, to publish its own objects over ubus
proxy. This way of testing allows ubus-x to be tested using a single docker
without any additional docker or network configuration.

##### test_proxied_objects

###### Description

Perform a ubus lookup and verify that there prefixed objects available on the
bus.

###### Test Steps

Use the ubus API call `ubus_lookup(4)`, providing `NULL` as path argument
for a wildcard lookup, parsing the output in the callback for an object
containing `/`, incrementing the test environment `__remote_obj_counter` if such
an object is found.

###### Test Expected Results

The expected result is for the `__remote_obj_counter` to not be 0.

##### test_proxied_access_ubusx_acl_success

###### Description

Attempt to lookup an object, which the ubus-x access lists explicitly allow
access to, and attempt to invoke one of its methods, verifying that the invoke
is successful.

###### Test Steps

Use the ubus API call `ubus_lookup_id(3)`, to find the `template` object,
prefixed by its IP (localhost), and invoke its method `test` through
`ubus_invoke(7)`.

###### Test Expected Results

The expected result is for the `invoke_cb(3)` to be triggered and increment the
context variable `__successful_invoke`.

##### test_proxied_access_ubusx_acl_fail

###### Description

Attempt to lookup an object which the access lists does not explicitly give
access to, and attempt to invoke one of its methods, verifying that the invoke
is unsuccessful.

###### Test Steps

Use the ubus API call `ubus_lookup_id(3)`, to find the `restricted_template`
object, prefixed by its IP (localhost), and invoke its method `test` through
`ubus_invoke(7)`.

###### Test Expected Results

The expected result is for no object id to be found, and for the invoke to fail,
leaving the context variable `__successful_invoke` set to 0.

##### test_proxied_events_acl_fail

###### Description

Open a listeners for events of the type `restricted_template`, prefixed by its
IP (localhost), which are published every two seconds by the `template` process,
and verify that no such events are received.

###### Test Steps

Use the ubus API call `ubus_register_event_handler(3)` to register a listener to
the event type, then schedule an interrupt callback on uloop through
`uloop_timeout_set(2)`, which sends an interrupt signal to `self()` after four
seconds, then let uloop take the process through `uloop_run()`. If an event is
recorded, interrupt uloop, else it will be interrupted after four seconds from
the scheduled interupt. Once interrupted the test results may be evaluated.

###### Test Expected Results

The expected result is for no event to be received and the interrupt to be
triggered after four seconds.

##### test_proxied_events_acl_success

###### Description

Open a listeners for events from `restricted_template`, prefixed by its IP
(localhost), which are published every two seconds by the `template` object,
and verify that the event is received.

###### Test Steps

Use the ubus API call `ubus_register_event_handler(3)` to register a listener to
the event type, then schedule an interrupt callback on uloop through
`uloop_timeout_set(2)`, which sends an interrupt signal to `self()` after four
seconds, then let uloop take the process through `uloop_run()`. If an event is
recorded, interrupt uloop, else it will be interrupted after four seconds from
the scheduled interupt. Once interrupted the test results may be evaluated.

###### Test Expected Results

The expected result is for such an event to be received and uloop to be
interrupted from the registered callback handler.

##### test_call_toggling_object

###### Description

The object `toggling_object` is repeatedly being published and unpublished on
ubus over ubus, and by extension, ubus-x. Attempt to call it to verify that
objects are being brought up during runtime can be accessed.

###### Test Steps

Invoke the toggling method `toggling_template->test`, if it fails, try again in
one second, for up to four seconds.

###### Test Expected Results

As the toggling object is brought up and down in three second intervals, it is
expected for the invoke to successfully go through.

##### test_observe_toggling_add

###### Description

Add a listener for `ubus.object.add` and observe for the argument to be
`toggling_template` prefixed by localhost, to verify that objects are accurately
being added to ubus when they are brought up.

###### Test Steps

Use the ubus API call `ubus_register_event_handler(3)` to register a listener
for `ubus.object.add`, schedule an interrupt callback on uloop through
`uloop_timeout_set(2)`, which sends an interrupt signal to `self()` after six
seconds, then let uloop take the process through `uloop_run()`. If an event is
recorded, interrupt uloop, else it will be interrupted after six seconds from
the scheduled interupt. Once interrupted evaluate the results from the test
environment context.

###### Test Expected Results

As the toggling object is brought up and down in three second intervals, it is
expected for an `ubus.object.add` event to be observed.

##### test_observe_toggling_remove

###### Description

Add a listener for `ubus.object.remove` and observe the argument to be
`toggling_template` prefixed by localhost, to verify that objects are accurately
being removed from ubus when they are brought up.

###### Test Steps

Use the ubus API call `ubus_register_event_handler(3)` to register a listener
for `ubus.object.remove`, schedule an interrupt callback on uloop through
`uloop_timeout_set(2)`, which sends an interrupt signal to `self()` after six
seconds, then let uloop take the process through `uloop_run()`. If an event is
recorded, interrupt uloop, else it will be interrupted after six seconds from
the scheduled interupt. Once interrupted evaluate the results from the test
environment context.

###### Test Expected Results

As the toggling object is brought up and down in three second intervals, it
the expected behavior is for an `ubus.object.remove` event to be observed.

##### test_ubusx_acl_init

###### Description

Dummy test case to exercise the function `ubusx_acl__init(0)`.

###### Test Steps

Invoke the method `ubusx_acl__init(0)` from the compiled shared library of the
owsd source files.

###### Test Expected Results

The expected result is for nothing to happen.

#### Ubus Tests

The ubus tests stress the ubus API and are performed using the libubus API.

| Serial ID		| Test Case Name	                                        |
| :---			| :---                                                      |
| 1				| [test_ubus_add](#test_ubus_add)                           |
| 2				| [test_ubus_remove_by_ip](#test_ubus_remove_by_ip)         |
| 3				| [test_ubus_remove_by_index](#test_ubus_remove_by_index)   |
| 4				| [test_ubus_retry](#test_ubus_retry)                       |

##### Prerequisites

For the ubus tests there are no prerequisites other than to have an active
instance of owsd running in the environment.

##### test_ubus_add

###### Description

Invoke `owsd.ubusproxy->add` on a dummy IP address to let owsd attempt to
connect to that address.

###### Test Steps

Use the libubus API to programatically invoke the `add` method with a dummy IP
as argument, then verify that the connection is attempted to be established
through `owsd.ubusproxy->list`.

###### Test Expected Results

The expected result is for the dummy IP `111.111.111.111` to be observed in the
response from `owsd.ubusproxy->list`.

##### test_ubus_remove_by_ip

###### Description

Add an address through `owsd.ubusproxy->add`, verify that it is added through
`owsd.ubusproxy->list` and try to remove it through `owsd.ubusproxy->remove`,
using the IP as an argument, verifying that the `remove` functionality is
properly implemented.

###### Test Steps

Use the prior established test, `test_ubus_add`, to add and verify the newly
added address, and then use the libubus API to invoke `owsd.ubusproxy->remove`,
and invoke `owsd.ubusproxy->list` to verify that the address is no longer in the
list.

###### Test Expected Results

The expected result is for the address to not be in the list (alternatively, be
in a teardown state) after the `owsd.ubusproxy->remove` invoke has been issued.

##### test_ubus_remove_by_index

###### Description

Add an address through `add`, verify that it is added through `list` and try
to remove it through `owsd.ubusproxy->remove`, using the index of the added
remote client as an argument.

###by generating HTTP requests
throught Steps

Similarily to `test_ubus_remove_by_ip`, use the test `test_ubus_add` to add a
dummy IP and verify it, then remove it through `owsd.ubusproxy->remove`, but
in this test via its index in the list.

###### Test Expected Results

The expected result is for the address to not be in the list (alternatively, be
in a teardown state) after the `remove` invoke has been issued.

##### test_ubus_retry

###### Description

Verify that owsd will continuously retry clients to which connection is not
established on the first attempt.

###### Test Steps

Add a dummy client through `test_ubus_add`, give it 30 seconds to reattempt the
connection, and verify through `owsd.ubusproxy->list` that the reconnect count
has been incremented. After which, try to remove the client.

###### Test Expected Results

The expected result is that the client will be added, not establish a connection
and be reattempted at least once. When remove is called, it is expected that the
client is removed or placed in a teardown state.

#### HTTP Tests

The http tests test the serving capabilities of owsd by generating HTTP requests
through libcurl.

| Serial ID		| Test Case Name	                                        |
| :---			| :---                                                      |
| 1				| [test_get_url](#test_get_url)                             |

##### Prerequisites

To test the http serving functionality of owsd, a file to serve has to be
found under `/www/`.

##### test_get_url

###### Description

Creates an HTTP request, and request a file to be served, verifying that it is
the same file as is found under the configured file servering directory, `www`.

###### Test Steps

Use libcurl to prepare an HTTP request and request the file `index.html`.

###### Test Expected Results

The file `index.html` to be served and found in the local folder, and its
contents to be the same as that in `/www/index.html`

### Functional API Tests

The OWSD API tests are done using the IOPSYS ubus-API-validation tool, only
specifying a JSON with objects, methods and arguments.

#### Prerequisites
For the API tests, two processes must be running in the environment:

- `ubusd` - Allowing ubus objects to be published.
- `owsd` - Hosting the ubus binaries under test.

Additionally, the ubus-API-validation tool and a JSON file detailing the input
arguments is necessary see [docs](https://dev.iopsys.eu/training/iopsyswrt-module-development/-/blob/master/testing/api-validation.md).

#### Ubus API tests

Only ubus methods which provides an output may be tested through the
ubus-API-validation tool, meaning there is only one test case.

| Execution ID	| Method      	| Description 			|
| :---			| :--- 	  		| :---					|
| 1				| list       	| No argument	  		|

## Writing New Tests

When writing new tests its first important to identify which type of test it is
and which requirement it covers. Most likely, the test is a functional test,
meaning it belongs in one of the functional test suites, however, should it be
a unit test, implement a new C test file using Cmocka, and add it to the
`./test/cmocka/CMakeLists.txt` under a unit-test suite.

### RPC

If a new RPC feature is implemented to be tested, it belongs in the
`./test/cmocka/rpc_test.c` source file. The RPC tests are implemented using
libwebsockets, and are not order agnostic, it is important that the test order
is maintained, and tests be appended to the end of the test list. The RPC
request logic should be prepared under the callback flag
`LWS_CALLBACK_CLIENT_WRITEABLE` and the corresponding ID parsed. Make sure to
identify the currently largest ID used, and increment that for each
libwebsocket `lws_callback_on_writable(1)` API call.

The reponse will be parsed in `LWS_CALLBACK_CLIENT_ESTABLISHED`, in worst case,
the response _can_ be parsed by its ID, which will match the one in the request.
To initiate polling of the websocket and reading of a RPC request response, use
the libwebsocket function `lws_service(2)`, which will read the websocket fd and
if anything is waiting, invoke the callback handler. Note that it may take time
for the RPC to go through and a timeout and a loop may be required.

### Ubus-X

For ubus-x tests, it is necessary to use libubus to make judgements whether the
functionality is achieved. When writing new tests, if it is event basesd and
a ubus listener is required, it is recommended to schedule an interrupt signal
to its own process, to take the thread back from uloop, as its event loop may
only be interrupted through signal handling.

### Ubus

When the ubus API of OWSD is extended, add additional tests to the ubus test
suite, `,/test/cmocka/ubus_test.c`, invoking the objects on ubus through the
libubus API with the aim to reach a high function coverage.

### HTTP

In the case of HTTP request tests being extended, the http test suite may be
extended at `./test/cmocka/http_test.c`. Use the C curl library, libcurl to form
the http requests.