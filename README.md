# OpenWrt Web Socket Daemon

The OpenWrt Web Socket Daemon (OWSD) is a webserver written in C, primarily
implemented with libwebsockets, to offer support for Ubus over HTTP and serving
files over websocket and HTTP.

## Overview
OWSD opens websocket listeners and allows for clients to connect to the
socket and issue remote procedure calls (RPC), in JSON-RPC format, compatible
with [uhttpd-mod-ubus](https://wiki.openwrt.org/doc/techref/ubus#access_to_ubus_over_http).
The RPCs are routed to the local intra-process communication bus available on
the system (primarily, ubus, but OWSD offers _beta_ D-Bus support). In addition
to issuing calls and receiving a response, OWSD also offers RPC support for
subscribing and receving events, and listing available objects.

Access control in OWSD is done at multiple levels, the most basic form being the
RPC access control issued by the session object (hosted by rpcd), but OWSD also
implements its own access lists, read from its UCI configuration, acting as a
white-list of remotely accessible objects.

Basic HTTP file serving is available in OWSD, and acts as a web back-end for the
JUCI Web UI on OpenWrt based platforms.

## UCI Configuration

Below is an example configuration, with its sections briefly explained in the
table, for a more detailed explanation see [developer docs](#./docs/README.md).

| Section Type 		| Description                                       						          |
| --------------- | ----------------------------------------------------------------------- |
| owsd        		| Global settings such as ubus socket, path to serve for http requests    |
| ubusproxy    		| Ubus proxy (ubus-x) settings, to be set from the _master_	access point  |
| owsd-listen  		| Interfaces to open websocket listeners at    					 	                |

```
config owsd 'global'
	option sock '/var/run/ubus.sock'
	option www '/www'

config ubusproxy 'ubusproxy'
	option enable '1'
	list peer 'wss://192.168.1.101/'
	list object 'router.*'
	list object 'system'
	option peer_cert '/etc/ssl/certs/owsd-repeater-control-cert.pem'
	option peer_key '/etc/ssl/private/owsd-repeater-control-key.pem'
	option peer_ca '/etc/ssl/certs/owsd-server-ca.pem'

config owsd-listen 'loopback'
	option port '80'
	option interface 'loopback'
	option ipv6 'on'
	list origin '*'

config owsd-listen 'lan'
	option port '80'
	option interface 'lan'
	option ipv6 'on'
	option whitelist_interface_as_origin '1'
	option whitelist_dhcp_domains '1'

config owsd-listen 'wan_https'
	option port '443'
	option interface 'wan'
	option cert '/etc/ssl/certs/cert.pem'
	option key '/etc/ssl/private/key.pem'
	option ca '/etc/ssl/certs/owsd-ca-for-clients.pem'
	option whitelist_interface_as_origin '1'
	list origin '*'

config owsd-listen 'wan'
	option port '80'
	option interface 'wan'
	option ipv6 'on'
	option whitelist_interface_as_origin '1'
	list origin '*'
```

## Ubus API

While OWSD does expose a ubus API, it is not necessarily meant for user
interaction, rather for [uproxyd](#ubus_proxy_daemon) to invoke when it
discovers potential ubus proxy hosts.

```
'owsd.ubusproxy' @07e1a8c1
	"add":{"ip":"String","port":"Integer"}
	"remove":{"index":"Integer","ip":"String"}
	"list":{"index":"Integer"}
```

## Supported RPCs

There are five exposed features through RPC:

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

## Ubus Support
- methods on ubus objects can be called via the "call" rpc
- events sent via `ubus_send_event can be received
- ACL checks are made prior to calling methods on ubus objects - the ubus session object is accessed to verify if session ID field has access
- ACL checks are also made prior to notifying clients of events they are listening for - "owsd" is used as the scope to check for "read" permission on the event

## Ubus Proxy Support - Networked Ubus
Through OWSD a networked version of Ubus is implemented, ubus proxy, or ubusx
for short. Between two OpenWrt hosts, a _client_ with ubus proxy support may
connect to a remote _server_, and list its available objects. Remotely available
objects may then be created on the local _clients_'s ubus, invoking the methods
of these (_stub_) objects, results in RPC calls to the _server_'s objects, thus
proxying objects over the network between two hosts.

```
$ ubus list 192*
192.168.1.121/netmode
```

## D-Bus support (_beta_)
In the case of both D-Bus and ubus support are enabled, D-Bus is searched first
to find the object being called.

When calling D-Bus methods, there are limitations with regard D-Bus argument
types, since goal is to maintain syntax, RPC format and types and stay ubus
compatible:
  * the RPC format specifies _object_ and _method_ name, while D-Bus requires _service_, _object_, _interface_ and _method_ . For D-Bus object to be available via RPC:
    - _service_ name must begin with compile-time specified prefix
    - the _interface_ must be same as _service_ name
    - and the _object_ path must begin with same compile-time specified prefix
  * the argument types supported include integer, string, and array of int or string; support for some more complex types can be achieved, but full type support is not possible in the general case if keeping ubus compatibility and RPC format


## SSL options
- if SSL support is available at compile-time in libwebsockets, SSL can be used
- server can be configured to allow all operations (skipping ubus session ACL checking) if clients provide a client certificate under CA trusted by server

## Manual Test Run

If you run the owsd as RPC server to listen on some port (e.g. 1234)

`owsd -p 1234`

then it is easiest to test/connect with a tool like `wscat`:

`wscat -c 'ws://127.0.0.1' -s ubus-json`

The established web socket can be used to send RPCs in the JSON format.

## Automated Testing

Four test suites have been prepared, to test ubusx, rpcd, ubus and http serving
functionality. Ubus-x and rpc tests exercise similar scope, through different
means.

Roughly speaking the rpc and ubus-x test cases are:
1. Check for access to remote device (ubus list for ubusx tests, rpc login for rpc tests)
2. Make a remote method call
3. Make a remote method call to an object without access rights
4. Listen for a remote event
5. Listen to a remote event type without access rights

Additionally, in rpc:
1. Test login
2. In/valid in/output formats
3. Parse errors
4. Subscribe-list

In the ubus-x tests, additional tests are added
to observe:
1. objects going up and down, with and without access lists.

Ubus test cases test the `owsd.ubusproxy` object:
1. Add
2. Remove by ip
3. Remove by index
4. Reconnect timer

## Dependencies

The minimum required depdnencies to compile owsd are:

| Dependency  		| Link                                       						              | License        |
| --------------- | -------------------------------------------------------------------	| -------------- |
| libwebsockets		| https://libwebsockets.org/                   					 	            | MIT            |
| libubox     		| https://git.openwrt.org/project/libubox.git 					 	            | BSD            |
| libubus     		| https://git.openwrt.org/project/ubus.git    					 	            | LGPL 2.1       |
| libjson-c   		| https://s3.amazonaws.com/json-c_releases    					 	            | MIT            |

With some optional dependencies:

| Dependency  		          | Link                                       				| License        |
| ------------------------- | ------------------------------------------------- | -------------- |
| libjson-validator  		    | https://git.openwrt.org/project/ubus.git    			|                |
| libjson-schema-validator  | https://git.openwrt.org/project/ubus.git    			|                |
| libdbus                   | https://www.freedesktop.org/wiki/Software/dbus/		| GPL            |
| libxml2                   | http://xmlsoft.org/		                            | MIT            |

Additionally, in order to build with the tests, the following libraries are needed:

| Dependency  				| Link                                       				        | License       |
| ------------------- | --------------------------------------------------------- | ------------- |
| cmocka             	| https://cmocka.org/                                    	  | Apache    		|
| libjson-editor			| https://dev.iopsys.eu/iopsys/json-editor   				        | 	            |


# Ubus Proxy Daemon
The ubus proxy daemon, uproxyd, is an application which is responsible for
detecting potential remote ubus proxy hosts. The reason for having a separate
daemon responsible for detecting remote hosts is to let owsd be agnostic to the
underlaying system, not depending on any other ubus objects to be present.

## Overview
Uproxyd automates detection of devices which supports ubusproxy in the network
and invoke `owsd.ubusproxy->add` on their IP address, allowing OWSD to attempt
to establish a websocket connection to the remote host. Uproxyd is also
responsible for notifying owsd of expired hosts, to which a connection may not
be successfully established.

The detection of potential remote hosts is dependent on the ubus method
`router.network->hosts`, and `client` events, both published by questd.

## Manual Tests
In iopsysWRT, `client` events and `router.network hosts` are published by the `questd` daemon. In the `./res/` subdirectory are two scripts (`client.sh` and `router.network`) which will simulate the functionality of `router.network hosts`, allowing `uproxyd` to be used without `questd` on the system.

To test ubus-x, run `client.sh` as a background process, place `router.network` in `/usr/libexec/rpcd/` and issue `/etc/init.d/rpcd restart`. Rpcd should now publish the object `router.network` and owsd/uproxyd can be restarted through `/etc/init.d/owsd restart`. You should now be able to access remote objects through ubus.


| Dependency  		| Link                                       						              | License        |
| --------------- | -------------------------------------------------------------------	| -------------- |
| libubox     		| https://git.openwrt.org/project/libubox.git 					 	            | BSD            |
| libubus     		| https://git.openwrt.org/project/ubus.git    					 	            | LGPL 2.1       |-