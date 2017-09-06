# LogZilla Rewrite Rules

# Release Version
LogZilla v5.75.1

# Parser Rules

Parser rules are used to change field values or to add user tags to events sent into the parser.

# Rule Files

Rules are loaded from JSON files placed in `/etc/logzilla/rules.d/` (default, can be changed)

# Rule Overview

Each rule must define a `match` condition and at least one of the following:

- `update`: a key-value map of fields and their eventual values
- `drop`: a boolean flag indicating the matched event should be ignored/dropped (not inserted into LogZilla).

## Basic Rule Example


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


# Rule Syntax

## Match Conditions

* `match` may be a single condition or an array of conditions.
* If `match` is an array, it will only match if **ALL** conditions are met (implied `AND`).
* Each condition must define a `field` and `value` along with an optional `op` (match operator).
* `value` may be a string or an array of strings.
* If `value` is an array, the condition will be met if **ANY** element of the array matches (implied `OR`).

### Valid `match` examples:

```json

"match": [
    { "field": "program", "value": ["program_a", "program_b"] },
    { "field": "host", "op": "ne" "value": "127.0.0.1" },
    { "field": "message", "op": "=~" "value": "\d+foo\s?bar" },
]
```

## Operators
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


# Rewriting Fields
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


### RegEx Rewrite Example

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

# Key/Value Example

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

## The `Update` keyword

The `update` keyword may also be used to "recall" any of:

1. Message (the message itself)
2. Host - The host name
3. Program - The program name

### `Update` Example

```json
"update": {
    "message": "$PROGRAM run on $HOST: $MESSAGE",
},
```

## Dropping events - `drop` keyword

To completely ignore events coming into LogZilla, use `"drop": true`.

This can be used to remove noise and only focus on important events.

> Note that `drop` cannot be used with any keyword except `match`.

### Drop example

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


## Skipping after first match - `first_match_only` flag

The `first_match_only` flag tells the Parser to stop trying to match events on each rule of the rule file after the first time it matches.

This can be useful when there is a need to update a field based on array of rules, but they are mutually exclusive.

Note that `first_match_only` is not an option of a singular rule, but whole rule file

### First match only example

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


# Rule Order

* All JSON rules files contained in `/etc/logzilla/rules.d/` are processed in alphabetical order.
* The Rules contained in each file are processed sequentially.
* If there are multiple rules with the same matching criteria, the last rule wins.

## Rule Order Example

**file1.json**

>Note: the JSON below contains `#### ruleN` comments which are not normally allowed in JSON syntax. These comments are used here for clarification on which rules would match.

```json

{
    "rewrite_rules": [
        {
            #### rule1
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
            #### rule2
            "match": {
                "field": "host",
                "value": "host_a"
            },
            "update": {"program": "new_program_name2"}
        }
    ]
}
```
### Result

Events matching the filters above will have the following properties.

```json
{
    ...
    "program": "new_program_name2", #### rule2
}
```



# Example: Wannacry Malware IoC's

These files should be stored as `/etc/logzilla/rules.d/wannacry.json` and `/etc/logzilla/rules.d/ip-blacklist.json` respectively. 

After adding/modifying files in the rules.d directory, run `sudo supervisorctl restart ParserProxy` so that the new entries are read into the system.

The examples below shows matches for all known `WannaCry` malware strings as well as all known `WannaCry Blacklisted IP Addresses`.

**Indicator of Compromise: WannaCry:**

```json
{
  "rewrite_rules": [
    {
      "filter": {
        "field": "message",
        "value": [
          "43bwabxrduicndiocpo.net",
          "57g7spgrzlojinas.onion",
          "76jdd2ir2embyv47.onion",
          "bcbnprjwry2.net",
          "bqkv73uv72t.com",
          "bqmvdaew.net",
          "chy4j2eqieccuk.com",
          "cwwnhwhlz52maqm7.onion",
          "dyc5m6xx36kxj.net",
          "easysupport.us",
          "ecoland.pro",
          "fa3e7yyp7slwb2.com",
          "fkksjobnn43.org",
          "graficagbin.com.br",
          "gurj5i6cvyi.net",
          "gx7ekbenv2riucmf.onion",
          "holdingair.top",
          "ju2ymymh4zlsk.com",
          "lkry2vwbd.com",
          "lvbrloxvriy2c5.onion",
          "ow24dxhmuhwx6uj.net",
          "palindromus.top",
          "r2embyv47.onion",
          "rbacrbyq2czpwnl5.net",
          "rphjmrpwmfv6v2e.onion",
          "rzlojinas.onion",
          "sdhjjekfp4k.com",
          "serionbrasil.com.br",
          "sqjolphimrr7jqw6.onion",
          "sxdcmua5ae7saa2.net",
          "trialinsider.com",
          "wwld4ztvwurz4.com",
          "www.bancomer.com.mx",
          "xanznp2kq.com",
          "xxlvbrloxvriy2c5.onion"
        ]
      },
      "field": "program",
      "value": "IoC-WannaCry"
    }
  ]
}
```	
**Indicator of Compromise: IP Blacklist:**

```json
{
"rewrite_rules": [
    {
      "filter": {
        "field": "message",
        "value": [
          "104.131.84.119",
          "128.31.0.39",
          "136.243.176.148",
          "144.76.92.176",
          "146.0.32.144",
          "148.244.38.101",
          "149.202.160.69",
          "158.69.92.127",
          "163.172.149.155",
          "163.172.153.12",
          "163.172.185.132",
          "163.172.25.118",
          "163.172.35.247",
          "167.114.35.28",
          "171.25.193.9",
          "176.9.39.218",
          "176.9.80.202",
          "178.208.83.16",
          "178.254.44.135",
          "178.62.173.203",
          "185.97.32.18",
          "188.138.33.220",
          "188.166.23.127",
          "192.42.113.102",
          "192.42.115.102",
          "193.11.114.43",
          "193.23.244.244",
          "195.22.26.248",
          "198.199.64.217",
          "198.96.155.3",
          "199.254.238.52",
          "212.47.232.237",
          "213.239.216.222",
          "213.61.66.116",
          "213.61.66.117",
          "217.172.190.251",
          "217.79.179.177",
          "217.79.179.77",
          "2.3.69.209",
          "38.229.72.16",
          "46.101.142.174",
          "46.101.166.19",
          "47.91.107.213",
          "50.7.151.47",
          "50.7.161.218",
          "51.15.36.164",
          "51.255.203.235",
          "51.255.41.65",
          "62.138.10.60",
          "62.138.7.171",
          "62.138.7.231",
          "62.210.124.124",
          "74.125.104.145",
          "79.172.193.32",
          "81.19.88.103",
          "81.30.158.223",
          "82.94.251.227",
          "83.162.202.182",
          "83.169.6.12",
          "86.59.21.38",
          "89.40.71.149",
          "89.45.235.21",
          "91.121.65.179",
          "91.219.237.229",
          "94.23.173.93"
        ]
      },
      "field": "program",
      "value": "IoC-IP_Blacklist"
    }
  ]
}
```


In these examples, any incoming events matching the strings will be inserted into LogZilla with a `Program` name of `IoC-WannaCry` and any of the blacklisted IP's will have a `Program` name of `IoC-IP_Blacklist`.

This allows you to set filters in LogZilla based on these Program names, for example:

##### Filter Example for WannaCry Program Field
![WannaCry-Blacklist](http://i.imgur.com/etE06sT.png)


##### WannaCry Dashboard
Creating a dashboard based on these names now becomes very easy:

![WannaCry](http://i.imgur.com/Rtx52os.png)

# WannaCry Alerts to Slack.com
Alerts are just as easy!

In the example below, we set a trigger alert for the `IoC-IP_Blacklist` rule and forward matched events to a Slack.com board.

![Trigger](http://i.imgur.com/Qe6HXUy.png)

##### Slack.com Alert

Here is what the Slack message looks like when we receive it from LogZilla:

![Imgur](http://i.imgur.com/9EAXQ9s.png)

<sub>
Note:
The files provided on Github are either contributed by us or the community, they come with no warranty and should not be considered production quality unless you have personally tested and approved them in your environment.</sub>

