# Purpose

`tsv2NEO` is used to create a NEO rule based on the input from a tab separated file.

**Usage:**

```
cat test.tsv | ./tsv2NEO
```

or, if you have `jq` installed, pretty print it using:

```
cat test.tsv | ./tsv2NEO | jq .
```

To add it to NEO:

```
cat test.tsv | ./tsv2NEO > 000-missing-devices.json
logzilla rules add 000-missing-devices.json
```

# Description

The script is written to use the following TAB separated columns:

```
devName <tab> devMAC1 <tab> devMAC2 <tab> devSerial <tab> contactFileNo <tab> contactName <tab> contactPhone <tab> contactMobile <tab> contactEmail <tab> searchType <tab> notes
```

**example:**

```
Bobs Laptop	A4:4C:C8:a1:Fc:30	B0:35:9F:3E:a9:f9	12345A7	17-11282a	Detective Picklepants	555-555-5556	555-555-5556	picklepants@dragnet.com	stolen	Latitude 3215 2-in-1 tablets stolen from Cafeteria Nov. 14-16th 2018
```
