# Function Specification

OWSD is a webserver written in C with the purpose of handling remote procedure
calls, serving files and extending the local intra-process communication bus,
over websocket connections.

## Requirements

Four basic user requirements were identified for rulengd.

| Requirement												        |
| :---														        |
| [Open WebSocket Listeners](#open_websocket_listeners)		        |
| [Accept Remote Procedure Calls](#accept_remote_procedure_calls) 	|
| [Serve Files Over HTTP](#serve_files_over_http)       			|
| [Proxy Ubus Objects](#proxy_ubus_objects)     					|

### Open WebSocket Listeners

The first requirement for OWSD is to act as a webserver and be able to open
websocket listeners. The library used for this purpose is libwebsockets, a
lightweight open-source library developed in C.

#### Why

By having a webserver on the system, clients may open a connection to the
device via browser or any websocket library, agnostic to underlying back-end,
and be served files and otherwise communicate with the device in a secure
manner.

#### How

Through libwebsockets, APIs are exposed to open websockets listeners by passing
structures holding port, interface and protocol information. A websocket
listener is known as a virtual host, `struct lws_vhost`, a private structure in
libwebsockets, created, configured and managed through libwebsocket API calls.

```
struct lws_vhost *lws_create_vhost(struct lws_context *context,
        const struct lws_context_creation_info *info);
```

The two structs, `struct lws_context` and `struct lws_context_creation_info`,
used to create virtual hosts, are prepared using information from owsd
input arguments, alternatively, from the owsd json configuration file. The
`lws_context` holds global context configuration, such as uid, gid, server name
etc. The `lws_context_creation_info` structure is prepared with port number,
`-p` flag, and interface, `-i` flag, to create the listener on, for more options
see [developer docs](#./README.md). For each virtual host created from the
`owsd-listen` section of the UCI configuration, there are two supported
protocols, http and ubus, with websocket connections being managed in each
protocols corresponding callback method.

### Accept Remote Procedure Calls

One of the responsibilites of the webserver is to accept remote procedure calls
(RPC), meaning owsd should be able to route websocket traffic provided in the
JSON-RPC format to the local bus, and answer with the response back over the
websocket.

#### Why

One of the features supported in OpenWrt is remote access to the local bus
through HTTP, through the webserver uhttpd. For OWSD to replace uhttpd it should
support the same feature set.

#### How

Remote procedure calls are managed by the `ubus-json` protocol, defined in
`wsubus_proto`:

```
struct lws_protocols ws_protocols[] = {
    ws_http_proto,
    wsubus_proto,
    { }
};
```
```
struct lws_protocols wsubus_proto = {
	WSUBUS_PROTO_NAME,
	wsubus_cb,
	sizeof (struct wsu_peer),
	32768,    /* arbitrary length */
	0,    /* - id */
	NULL, /* - user pointer */
};
```
Meaning all websocket connections over any of the virtual hosts specifying the
`ubus-json` protocol will be managed by the `wsubus_cb(5)` callback. Any data
sent by a client connected over the `ubus-json` protocol will be sent with the
flag `LWS_CALLBACK_RECEIVE`. Any payload in the JSON-RPC format will be
routed to the appropriate action via `wsubus_rx_json(3)`, verifying that the
entire message has been sent through the websocket, and using `libjson-c` to
parse it. The OWSD RPC has been developed to conform with the OpenWrt RPC
standards, see [here](#https://oldwiki.archive.openwrt.org/doc/techref/ubus#access_to_ubus_over_http),
meaning it implements parsing to manage `list`, `method`, `subscribe`,
`unsubscribe` and `subscribe-list`.

See the `rpc_*.c` source files and [developer docs](#./README.md) for more
specifics on the RPC handling.

### Serve Files Over HTTP

#### Why

OpenWrt supports LuCI, which is served through the uhttpd webserver, similarily
OWSD should be able to act as backend for the iopsysWrt webGUI, JUCI. To
support this, OWSD has to have custom HTTP handling over websocket and HTTP
requests.

#### How

For serving HTTP requests, the `ws_http_proto` protocol is used, defined by:

```
struct lws_protocols ws_http_proto = {
	/* because we only want to use this callback/protocol for poll integration,
	 * management and pre-websocket stuff, we don't want any subprotocol name
	 * to match this and negotiate a connection under this subprotocol */
	",,,,,,,,",
	ws_http_cb,
	/* following other fields we don't use: */
	0,    /* - per-session data size */
	0,    /* - max rx buffer size */
	0,    /* - id */
	NULL, /* - user pointer */
};
```

As websockets allow clients to negotiate without a protocol name, and in those
instances, it will always connect the first protocol in the `ws_protocols` list.
Therefore, the http protocol must always be protocol index 0. The first protocol
serves multiple purposes, it deals with pre-websocket parsing such as file
descriptor assignation, but also HTTP requests. OWSD has implemented custom
HTTP serving logic, rather than using callbacks prepared through libwebsockets.

HTTP requests will come to `ws_http_cb(5)` with the flag `LWS_CALLBACK_HTTP`,
allowing owsd to parse the flag and serve files through `ws_http_serve_file(2)`,
which will serve a requested file, basd on redirect configurations and cache,
using the libwebsockets API. The actual serving of a file is done through
`lws_add_http_header_by_token(6)` to add a header, and `lws_serve_http_file(5)`
to initiate the transfer of data through the websocket.


### Proxy Ubus Objects

#### Why

With the support being added for multi access point networks, a secure and
convenient way to communicate between the access points is necessary. Seeing
as ubus is the primary form of intra-process communication on OpenWrt and
iopsysWrt platforms, it is sensical to offer ubus communication between the
devices. For this purpose OWSD, through libwebsockets, offers the possibility
for a secure way to communicate with remote hosts, hosting their own webserver
instance of OWSD.

#### How

To meet the final requirement of owsd, the bus of a remote host should be
extended to the local bus of another host on the network. This can be achieved
letting one host on the network connect to another host using
websockets.

An instance of OWSD running with ubusproxy enable in the UCI configuration, or
accordingly configured JSON or input flag (`-X`) will publish an object to ubus,
allowing other daemons to invoke add and remove functions, to let owsd know of
other potential hosts. The reason for this separation is to keep owsd agnostic
of the underlaying platform. Alternatively, if the remote host has a static
address, it can be defined in the `peer` list in the UCI configuration, or the
`-P` flag. On input or discovery of potential peer, a virtual host is prepared
to open a connection to the remote and scheduled through uloop to perform a
websocket connection via the libwebsockets API `lws_client_connect_via_info(1)`

The protocol used for proxying remote peers is `ws_ubusproxy_proto`.

```
struct lws_protocols ws_ubusproxy_proto = {
        WSUBUS_PROTO_NAME,
        ws_ubusproxy_cb,
        sizeof (struct wsu_peer),
        32768,    /* arbitrary length */
        0,        /* - id */
        NULL, /* - user pointer */
};
```

The protocols callback, `ws_ubusproxy_cb(5)`, has a process setup to login,
perform a ubus list and setup local stubs of the remote objects, which are then
published to the local bus, prefixed by the IP address of the remote host, i.e.
`192.168.1.244/test`. All requests to the stubs over the local bus will be
parsed and sent over the websocket.

