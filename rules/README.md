# LogZilla Rules

<sub>
Note:
The files provided on GitHub are either contributed by us or the community, they come with no warranty and should not be considered production quality unless you have personally tested and approved them in your environment.</sub>

# CAUTION: Some of these rules will create a large amount of entries in your system, MAKE SURE YOUR SERVER IS SCALED PROPERLY.

## Parser Rules

Parser rules are used to change field values or to add user tags to events sent into the parser.

## Rule Files

Rules are loaded from JSON files using the command `logzilla rules add somefile.json`

## Rule Overview

Each rule must define a `match` condition and at least one of the following:

- `update`: a key-value map of fields and their eventual values
- `drop`: a boolean flag indicating the matched event should be ignored/dropped (not inserted into LogZilla).

### Basic Rule Example


```json
{
    "match": {
        "field": "host",
        "value": [
            "host_a", "host_b"
        ]
    },
    "update": {
        "program": "new_program_name",
        "host": "new_host_name"
    }
}
```
In this example, the rule above changes the incoming event in the following manner:

1. Match on either `host_a` or `host_b`
2. Set the `program` field to `new_program_name`
3. Set the `host` field to `new_host_name`


## Rule Syntax

### Match Conditions

* `match` may be a single condition or an array of conditions.
* If `match` is an array, it will only match if **ALL** conditions are met (implied `AND`).
* Each condition must define a `field` and `value` along with an optional `op` (match operator).
* `value` may be a string or an array of strings.
* If `value` is an array, the condition will be met if **ANY** element of the array matches (implied `OR`).

#### Valid `match` examples:

```json

"match": [
    { "field": "program", "value": ["program_a", "program_b"] },
    { "field": "host", "op": "ne" "value": "127.0.0.1" },
    { "field": "message", "op": "=~" "value": "\d+foo\s?bar" },
]
```

### Operators
Operators control the way the `match` condition is applied. If no `op` is supplied, the default operator `eq` is assumed.

| Operator | Match Type        | Description                                                                                   |
|----------|-------------------|-----------------------------------------------------------------------------------------------|
| eq       | String or Integer | Matches entire incoming message against the string/integer specified in the `match` condition |
| ne       | String or Integer | Does *not* match anything in the incoming message `match` field.                              |
| gt       | Integer Only      | Given integer is greater than the incoming integer value                                      |
| lt       | Integer Only      | Given integer is less than the incoming integer value                                         |
| ge       | Integer Only      | Given integer is greater than or equal to the incoming integer value                          |
| le       | Integer Only      | Given integer is less than or equal to the incoming integer value                             |
| =~       | RegEx             | Match based on RegEx pattern                                                                  |
| !~       | RegEx             | Does *not* match based on RegEx pattern                                                       |
| =*       | RegEx             | RegEx appears anywhere in the incoming message                                                |


## Rewriting Fields
To transform an incoming event into a new string, use the `update` keyword.

When replacing incoming event parts, the rules can reuse events from the original field's values in three ways:

1. Capturing RegEx sub-matches
2. key/value parsing of the incoming MESSAGE field
3. Full string values of incoming MESSAGE, HOST and/or PROGRAM fields
4. Combinations of the above (i.e. these features may be used together in a single rule)

To replace parts from `field` RegEx operators in an `update`, one or more of its values must contain capture references.

These RegEx capture references **must not** be escaped.
**Example**: `$1`, `$2`, `$3`, etc.

- `$1` is the correct way to replace the value with the captured RegEx.
- `\$1` would match `$1` *literally* (and would not reference the RegEx captured).
- One (and exactly one) `match` condition must capture these sub-matches.
- The value must be a RegEx string with at least as many captures used by the `update` fields.
- The condition must have the `op` (operator) set as a RegEx operator, e.g.: `"=~"`.
- If the operator type (`op`) is excluded, `eq` will be assumed.


#### RegEx Rewrite Example

The following rule rewrites a `program` field on events `not` originating from the host named `127.0.0.1`.

1. Match on the `message` field
2. Use the RegEx operator of `=~`
3. Match on any message containing either of the strings set in the `value`
4. Do not consider this a match if the `host` is `127.0.0.1`
5. If the above criteria are met, set the `program` name to `$1` (the RegEx capture obtained from the `value` in the `match` statement).

```json
{
    "match": [
        {
            "field": "message",
            "op": "=~",
            "value": [
                "output of program (\w+)",
                "error while running (\w+)"
            ],
        }, {
            "field": "host", "value": "127.0.0.1", "op": "ne"
        }
    ],
    "update": {
        "program": "$1"
    }
}
```

## Key/Value Example

To use the key=value parser, one or more of the `update` fields must reference an unescaped key variable `${KEY_NAME}` from the incoming event. It will be replaced only if the text of the `message` matches. Note that at least one explicit `match` condition must still be applied.

For example, the following rule will rewrite the entire message of an incoming Juniper event (which uses key/value pairs).

Sample Original Incoming Message (before rewrite):

> Note: the sample message below is *only* the message itself and doesn't include the host, pri, or program.

```
2017-07-03T12:23:33.146 SRX5800 RT_FLOW - RT_FLOW_SESSION_CREATE [junos@2636.1.1.1.2.26 source-address="1.2.7.19" source-port="46157" destination-address="2.4.21.21" destination-port="443" service-name="junos-https" nat-source-address="6.12.7.29" nat-source-port="46157" nat-destination-address="1.3.21.22" nat-destination-port="443" src-nat-rule-name="None" dst-nat-rule-name="SSL-vpn" protocol-id="6" policy-name="SSL" source-zone-name="intn" destination-zone-name="dmz" session-id-2="3341217" username="N/A" roles="N/A" packet-incoming-interface="eth0.1"]
```

**Desired Outcome:**

1. Match on the incoming `message` field using a RegEx operator.
2. Rewrite the entire message using the defined string with key=value as well as the captured RegEx.
3. Set the `program` name to `Juniper`.
4. Create a second `match` condition and match on the `Juniper` program set in the first `match`.
5. Use RegEx to find out if the `message` contains the word *reason*
6. If it does contain a *reason* value, then add that *reason* to the message.


```json
{
  "rewrite_rules": [
    {
      "match": {
        "field": "message",
        "op": "=~",
        "value": "(\\S+) (\\S+) \\S+ - RT_FLOW_(SESSION_\\w+)"
      },
      "update": {
        "message": "$3 reason=${reason} src=${source-address} dst=${destination-address} src-port=${source-port} dst-port=${destination-port} service=${service-name} policy=${policy-name} nat-src=${nat-source-address} nat-src-port=${nat-source-port} nat-dst=${nat-destination-address} nat-dst-port=${nat-destination-port} src-nat-rule=${src-nat-rule-name} dst-nat-rule=${dst-nat-rule-name} protocol=${protocol-id} src-zone=${source-zone-name} dst-zone=${destination-zone-name} session-id=${session-id-32} ingress-interface=${packet-incoming-interface} $2 $1",
        "program": "Juniper"
      }
    },
    {
      "match": [
        {
          "value": "Juniper",
          "field": "program"
        },
        {
          "value": "(.+?) reason= (.+)",
          "field": "message"
        }
      ],
      "update": {
        "message": "$1 $2"
      }
    }
  ]
}
```

### The `Update` keyword

The `update` keyword may also be used to "recall" any of:

1. Message (the message itself)
2. Host - The host name
3. Program - The program name

#### `Update` Example

```json
"update": {
    "message": "$PROGRAM run on $HOST: $MESSAGE",
},
```

### Dropping events - `drop` keyword

To completely ignore events coming into LogZilla, use `"drop": true`.

This can be used to remove noise and only focus on important events.

> Note that `drop` cannot be used with any keyword except `match`.

#### Drop example

The following example shows how to completely ignore diagnostic messages from a program called `thermald`.

```json
{
    "rewrite_rules": [
        {
            "match": [
                { "field": "program", "value": "thermald"},
                { "field": "severity", "op": "ge", "value": 6}
            ],
            "drop": true
        },
    ]
}
```

Operator `"ge"` means `greater or equal`, so it only drops events of severity 6 (informational) and 7 (debug).


### Skipping after first match - `first_match_only` flag

The `first_match_only` flag tells the Parser to stop trying to match events on each rule of the rule file after the first time it matches.

This can be useful when there is a need to update a field based on array of rules, but they are mutually exclusive.

Note that `first_match_only` is not an option of a singular rule, but whole rule file

#### First match only example

```json
{
    "rewrite_rules": [
        {
            "match": { "field": "program", "value": "MSU_Makerbot"},
            "update": { "program": "Makerbot" }
        },
        {
            "match": { "field": "program", "op": "=~", "value": "[%#]([\w-#|]+?-\d+-\w+)" },
            "update": { "program": "Cisco" }
        },
        {
            "match": {
                "field":"message", "op":"=~",
                "value": "(\\d*)\\s+(MSWinEventLog|Microsoft-Windows|Windows)-(\\S+)"
            },
            "update": {"program": "MSWin-$3", "message":"EventID=$1 $MESSAGE"},
        }
    ],
    "first_match_only": true
}
```

With `first_match_only`, the Parser won't waste time and resources to try to match Makerbot and Cisco events to Windows-specific rules.
> Note that this flag only affects the scope of *this* current rule file (not all JSON files in `/etc/logzilla/rules.d/`. Regardless of whether or not any of these rules match, other rule files which do make a match will still be applied.


## Rule Order

* All JSON rules files contained in `/etc/logzilla/rules.d/` are processed in alphabetical order.
* The Rules contained in each file are processed sequentially.
* If there are multiple rules with the same matching criteria, the last rule wins.

### Rule Order Example

**file1.json**

```json

{
    "rewrite_rules": [
        {
            "comment": "rule1",
            "match": {
                "field": "host",
                "value": "host_a"
            },
            "update": {"program": "new_program_name"}
        }
    ]
}
```

**file2.json**

```json
{
    "rewrite_rules": [
        {
            "comment": "rule2",
            "match": {
                "field": "host",
                "value": "host_a"
            },
            "update": {"program": "new_program_name2"}
        }
    ]
}
```
#### Result

Events matching the filters above will have the following properties.

```json
{
    ...
    "program": "new_program_name2", #### rule2
}
```



