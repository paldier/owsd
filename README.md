# OWSD web socket daemon

## Overview
- uses web sockets to accept RPC calls in JSON-RPC
- intended as web back-end for JUCI Web UI on OpenWrt based platform
- RPCs are routed to local IPC bus objects on ubus and/or D-Bus (_beta_)
- supports receiving events in addition to issuing method calls
- uses ubus session object to perform access control
- JSON-RPC format is compatible with [uhttpd-mod-ubus](https://wiki.openwrt.org/doc/techref/ubus#access_to_ubus_over_http)
- basic HTTP file serving logic is available
- powered by libwebsockets, libubox, libubus

## Supported RPCs
- "list"
  * lists available object and methods; identical to [uhttpd-mod-ubus](https://wiki.openwrt.org/doc/techref/ubus#access_to_ubus_over_http)
- "call"
  * call method of an object; identical to [uhttpd-mod-ubus](https://wiki.openwrt.org/doc/techref/ubus#access_to_ubus_over_http)
- "subscribe"
  * start listening for broadcast events by glob (wildcard) pattern
- "subscribe-list"
  * list which events we are listening for
- "unsubscribe"
  * stop listening

## ubus support
- methods on ubus objects can be called via the "call" rpc
- events sent via ubus\_send\_event can be received
- ACL checks are made prior to calling methods on ubus objects - the ubus session object is accessed to verify if session ID field has access
- ACL checks are also made prior to notifying clients of events they are listening for - "owsd" is used as the scope to check for "read" permission on the event

## ubus proxy support - networked ubus
- using ubus proxy support, ubus objects can be proxied over the network across two hosts
- with two hosts, _client_ with ubus proxy support connects to remote _server_, and lists available objects
- remotely available objects are created on the local _client_'s ubus, calling methods of these (_stub_) objects results in RPC calls to the _server_'s objects
- see this [screencast](https://asciinema.org/a/3u1dl3ojggxih31wi495dr4zj)

## D-Bus support (_beta_)
- if both D-Bus and ubus support are enabled, D-Bus is searched first to find the object being called
- when calling D-Bus methods, there are limitations with regard D-Bus argument types, since goal is to maintain syntax, RPC format and types and stay ubus compatible
	* the RPC format specifies _object_ and _method_ name, while D-Bus requires _service_, _object_, _interface_ and _method_ . For D-Bus object to be available via RPC:
        - _service_ name must begin with compile-time specified prefix
        - the _interface_ must be same as _service_ name
        - and the _object_ path must begin with same compile-time specified prefix
    * the argument types supported include integer, string, and array of int or string; support for some more complex types can be achieved, but full type support is not possible in the general case if keeping ubus compatibility and RPC format


## SSL options
- if SSL support is available at compile-time in libwebsockets, SSL can be used
- server can be configured to allow all operations (skipping ubus session ACL checking) if clients provide a client certificate under CA trusted by server

## Manual test run

If you run the owsd as RPC server to listen on some port (e.g. 1234)

`owsd -p 1234`

then it is easiest to test/connect with a tool like `wscat`:

`wscat -c 'ws://127.0.0.1' -s ubus-json`

The established web socket can be used to send RPCs in the JSON format.

## Tests

In the test/ subdirectory, there is a very simple test runner made in nodejs. It is configured by editing parameters `config.js`, and running:

`node client.js [session-id]`

The `config.js` file specifies the text file containing test input and expected output. See existing text files for examples of RPC commands.

## Other resources: libwebsockets documentation

https://libwebsockets.org/lws-api-doc-master/html/md_README.coding.html
