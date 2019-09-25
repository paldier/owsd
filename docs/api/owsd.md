# OWSD Schema

```
http://example.com/root.json
```

| Custom Properties | Additional Properties |
| ----------------- | --------------------- |
| Forbidden         | Forbidden             |

# OWSD

| List of Methods   |
| ----------------- |
| [add](#add)       | Method | OWSD (this schema) |
| [list](#list)     | Method | OWSD (this schema) |
| [remove](#remove) | Method | OWSD (this schema) |

## add

`add`

- type: `Method`

### add Type

`object` with following properties:

| Property | Type   | Required |
| -------- | ------ | -------- |
| `input`  | object | Optional |
| `output` | object | Optional |

#### input

`input`

- is optional
- type: `object`

##### input Type

`object` with following properties:

| Property | Type    | Required |
| -------- | ------- | -------- |
| `ip`     | string  | Optional |
| `port`   | integer | Optional |

#### ip

`ip`

- is optional
- type: reference

##### ip Type

`string`

All instances must conform to this regular expression (test examples
[here](<https://regexr.com/?expression=%5E((25%5B0-5%5D%7C2%5B0-4%5D%5B0-9%5D%7C%5B01%5D%3F%5B0-9%5D%5B0-9%5D%3F)(%5C.%7C%24))%7B4%7D%24>)):

```regex
^((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)(\.|$)){4}$
```

#### port

`port`

- is optional
- type: `integer`

##### port Type

`integer`

- minimum value: `1`
- maximum value: `65535`

### Ubus CLI Example

```
ubus call OWSD add {"ip":"203.203236.249","port":29715}
```

### JSONRPC Example

```json
{
  "jsonrpc": "2.0",
  "id": 0,
  "method": "call",
  "params": ["<SID>", "OWSD", "add", { "ip": "203.203236.249", "port": 29715 }]
}
```

#### output

`output`

- is optional
- type: `object`

##### output Type

`object` with following properties:

| Property | Type | Required |
| -------- | ---- | -------- |
| None     | None | None     |

### Output Example

```json
{}
```

## list

`list`

- type: `Method`

### list Type

`object` with following properties:

| Property | Type   | Required |
| -------- | ------ | -------- |
| `input`  | object | Optional |
| `output` | object | Optional |

#### input

`input`

- is optional
- type: `object`

##### input Type

`object` with following properties:

| Property | Type | Required |
| -------- | ---- | -------- |
| None     | None | None     |

### Ubus CLI Example

```
ubus call OWSD list {}
```

### JSONRPC Example

```json
{ "jsonrpc": "2.0", "id": 0, "method": "call", "params": ["<SID>", "OWSD", "list", {}] }
```

#### output

`output`

- is optional
- type: `object`

##### output Type

`object` with following properties:

| Property  | Type  | Required     |
| --------- | ----- | ------------ |
| `clients` | array | **Required** |

#### clients

`clients`

- is **required**
- type: `object[]`

##### clients Type

Array type: `object[]`

All items must be of the type: `object` with following properties:

| Property          | Type    | Required     |
| ----------------- | ------- | ------------ |
| `SSL`             | boolean | **Required** |
| `index`           | integer | **Required** |
| `ip`              | string  | **Required** |
| `path`            | string  | **Required** |
| `port`            | integer | **Required** |
| `protocol`        | string  | **Required** |
| `reconnect_count` | integer | **Required** |
| `state`           | string  | **Required** |
| `type`            | string  | **Required** |

#### SSL

`SSL`

- is **required**
- type: `boolean`

##### SSL Type

`boolean`

#### index

`index`

- is **required**
- type: `integer`

##### index Type

`integer`

- minimum value: `0`

#### ip

`ip`

- is **required**
- type: reference

##### ip Type

`string`

All instances must conform to this regular expression (test examples
[here](<https://regexr.com/?expression=%5E((25%5B0-5%5D%7C2%5B0-4%5D%5B0-9%5D%7C%5B01%5D%3F%5B0-9%5D%5B0-9%5D%3F)(%5C.%7C%24))%7B4%7D%24>)):

```regex
^((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)(\.|$)){4}$
```

#### path

`path`

- is **required**
- type: `string`

##### path Type

`string`

#### port

`port`

- is **required**
- type: `integer`

##### port Type

`integer`

- minimum value: `0`
- maximum value: `65535`

#### protocol

`protocol`

- is **required**
- type: `string`

##### protocol Type

`string`

#### reconnect_count

`reconnect_count`

- is **required**
- type: `integer`

##### reconnect_count Type

`integer`

- minimum value: `0`

#### state

`state`

- is **required**
- type: `enum`

##### state Type

`string`

The value of this property **must** be equal to one of the [known values below](#list-known-values).

##### state Known Values

| Value        |
| ------------ |
| Disconnected |
| Connected    |
| Connecting   |

#### type

`type`

- is **required**
- type: `string`

##### type Type

`string`

### Output Example

```json
{
  "clients": [
    {
      "index": 58897343,
      "ip": "2041.96.236.",
      "port": 61575,
      "path": "voluptate elit velit qui",
      "protocol": "consequat",
      "SSL": true,
      "type": "pariatur",
      "state": "Connected",
      "reconnect_count": 70370122
    },
    {
      "index": 7625544,
      "ip": "14.210.23101.",
      "port": 28094,
      "path": "n",
      "protocol": "exercitation",
      "SSL": true,
      "type": "laboris deserunt adipisicing dolore exercitation",
      "state": "Connecting",
      "reconnect_count": 75314312
    },
    {
      "index": 99246884,
      "ip": "254.217252200",
      "port": 18169,
      "path": "ad nulla",
      "protocol": "esse",
      "SSL": true,
      "type": "officia irure se",
      "state": "Connected",
      "reconnect_count": 94690275
    },
    {
      "index": 70877858,
      "ip": "216.6.255.92",
      "port": 14023,
      "path": "ullamco",
      "protocol": "in",
      "SSL": false,
      "type": "minim ",
      "state": "Connected",
      "reconnect_count": 61372668
    },
    {
      "index": 11143022,
      "ip": "253.115.252246.",
      "port": 31748,
      "path": "consectetur",
      "protocol": "in sunt dolor",
      "SSL": false,
      "type": "velit est ex officia mollit",
      "state": "Connecting",
      "reconnect_count": 99562703
    }
  ]
}
```

## remove

`remove`

- type: `Method`

### remove Type

`object` with following properties:

| Property | Type   | Required |
| -------- | ------ | -------- |
| `input`  | object | Optional |
| `output` | object | Optional |

#### input

`input`

- is optional
- type: `object`

##### input Type

`object` with following properties:

| Property | Type    | Required |
| -------- | ------- | -------- |
| `index`  | integer | Optional |
| `ip`     | string  | Optional |

#### index

`index`

- is optional
- type: `integer`

##### index Type

`integer`

- minimum value: `0`

#### ip

`ip`

- is optional
- type: reference

##### ip Type

`string`

All instances must conform to this regular expression (test examples
[here](<https://regexr.com/?expression=%5E((25%5B0-5%5D%7C2%5B0-4%5D%5B0-9%5D%7C%5B01%5D%3F%5B0-9%5D%5B0-9%5D%3F)(%5C.%7C%24))%7B4%7D%24>)):

```regex
^((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)(\.|$)){4}$
```

### Ubus CLI Example

```
ubus call OWSD remove {"ip":"28225253255.","index":12987796}
```

### JSONRPC Example

```json
{
  "jsonrpc": "2.0",
  "id": 0,
  "method": "call",
  "params": ["<SID>", "OWSD", "remove", { "ip": "28225253255.", "index": 12987796 }]
}
```

#### output

`output`

- is optional
- type: `object`

##### output Type

`object` with following properties:

| Property | Type | Required |
| -------- | ---- | -------- |
| None     | None | None     |

### Output Example

```json
{}
```
