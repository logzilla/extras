# TSV2Meta

This script will take in a tab separated file and convert the entries to a LogZilla rule in JSON format

## Input fields

The `.tsv` file should contain columns for:

* addtag - indicate (0 or 1) whether or not a tag field should also be created for this key/value pair
* matchField - The field to match on, such as `host`, `program`, `message`, etc.
* matchValue - the value of the match field such as `my.host.com` or `1.2.3.4` if you are matching on `host` (from the `matchField` specified above)
* key=value - set the MetaTag keys and values you want for this match, for example: `Location=Raleigh` or multiple key=value pairs in comma separated format such as `SiteNumber=323,Location=Raleigh`

## Usage

```
cat test.tsv | ./tsv2meta
```

This will generate a minified `.json` rule, for example:

```
{"rewrite_rules":[{"match":[{"value":"10.1.2.35","op":"=~","field":"host"}],"tag":{"ut_meta_deviceid":"rtp-core-sw","ut_meta_devicelayer":"L2","ut_meta_devicelocation":"Raleigh","ut_meta_deviceimportance":"High","ut_meta_devicecontact":"support@logzilla.net","ut_meta_devicerole":"Core"},"update":{"message":"$MESSAGE DeviceID=\"rtp-core-sw\" DeviceDescription=\"RTP Core Layer2\" DeviceImportance=\"High\" DeviceLocation=\"Raleigh\" DeviceLayer=\"L2\" DeviceContact=\"support@logzilla.net\" DeviceRole=\"Core\""}},{"match":[{"value":"10.1.2.5","op":"=~","field":"host"}],"tag":{"ut_meta_deviceid":"rtp-core-rtr","ut_meta_devicelayer":"L3","ut_meta_devicelocation":"Raleigh","ut_meta_deviceimportance":"High","ut_meta_devicecontact":"support@logzilla.net","ut_meta_devicerole":"Core"},"update":{"message":"$MESSAGE DeviceID=\"rtp-core-rtr\" DeviceDescription=\"RTP Core ASR\" DeviceImportance=\"High\" DeviceLocation=\"Raleigh\" DeviceLayer=\"L3\" DeviceContact=\"support@logzilla.net\" DeviceRole=\"Core\""}},{"match":[{"value":"MPLS","op":"=~","field":"message"}],"update":{"message":"$MESSAGE DeviceType=\"ASR\""}}]}

```

To help make the result more readable, install `jq` on your system. For example, on an Ubuntu server, just type:

```
sudo apt install jq
```
This will allow you to "prettify" the output from the conversion, for example:


```
{
  "rewrite_rules": [
    {
      "match": [
        {
          "value": "10.1.2.35",
          "op": "=~",
          "field": "host"
        }
      ],
      "tag": {
        "ut_meta_deviceid": "rtp-core-sw",
        "ut_meta_devicelayer": "L2",
        "ut_meta_devicelocation": "Raleigh",
        "ut_meta_deviceimportance": "High",
        "ut_meta_devicecontact": "support@logzilla.net",
        "ut_meta_devicerole": "Core"
...(truncated for brevity)
```

### Use Case Sample
As a practical example, let's say we wanted to match on all incoming events that have a host name of `1.1.1.2`

We want to create a rule to add the following meta keys and values and create user tag fields for all except the MPLS message metatag

Create a tab separated file called `myfile.tsv` with the following entries:
> <font color="red"> IMPORTANT: </font> the spaces below are `tabs`, not spaces.

```
1	host	10.1.2.35	DeviceID=rtp-core-sw,DeviceDescription=RTP Core Layer2,DeviceImportance=High,DeviceLocation=Raleigh,DeviceLayer=L2,DeviceContact=support@logzilla.net,DeviceRole=Core
1	host	10.1.2.5	DeviceID=rtp-core-rtr,DeviceDescription=RTP Core ASR,DeviceImportance=High,DeviceLocation=Raleigh,DeviceLayer=L3,DeviceContact=support@logzilla.net,DeviceRole=Core
0	message	MPLS	DeviceType=ASR

```

Now type `cat myfile.tsv | ./tsv2meta`

Which results in a `.json` file containing the following (prettified below for readability):

```
{
  "rewrite_rules": [
    {
      "match": [
        {
          "value": "10.1.2.35",
          "op": "=~",
          "field": "host"
        }
      ],
      "tag": {
        "ut_meta_deviceid": "rtp-core-sw",
        "ut_meta_devicelayer": "L2",
        "ut_meta_devicelocation": "Raleigh",
        "ut_meta_deviceimportance": "High",
        "ut_meta_devicecontact": "support@logzilla.net",
        "ut_meta_devicerole": "Core"
      },
      "update": {
        "message": "$MESSAGE DeviceID=\"rtp-core-sw\" DeviceDescription=\"RTP Core Layer2\" DeviceImportance=\"High\" DeviceLocation=\"Raleigh\" DeviceLayer=\"L2\" DeviceContact=\"support@logzilla.net\" DeviceRole=\"Core\""
      }
    },
    {
      "match": [
        {
          "value": "10.1.2.5",
          "op": "=~",
          "field": "host"
        }
      ],
      "tag": {
        "ut_meta_deviceid": "rtp-core-rtr",
        "ut_meta_devicelayer": "L3",
        "ut_meta_devicelocation": "Raleigh",
        "ut_meta_deviceimportance": "High",
        "ut_meta_devicecontact": "support@logzilla.net",
        "ut_meta_devicerole": "Core"
      },
      "update": {
        "message": "$MESSAGE DeviceID=\"rtp-core-rtr\" DeviceDescription=\"RTP Core ASR\" DeviceImportance=\"High\" DeviceLocation=\"Raleigh\" DeviceLayer=\"L3\" DeviceContact=\"support@logzilla.net\" DeviceRole=\"Core\""
      }
    },
    {
      "match": [
        {
          "value": "MPLS",
          "op": "=~",
          "field": "message"
        }
      ],
      "update": {
        "message": "$MESSAGE DeviceType=\"ASR\""
      }
    }
  ]
}
```

The resulting rule would take an incoming event like:

```
10.1.2.35	189		%SYS-5-CONFIG_I: Configured from console by cisco on vty6 (149.121.24.9)
```

And append the new Metatags to the original message, like so:

```
10.1.2.35	189		%SYS-5-CONFIG_I: Configured from console by cisco on vty6 (149.121.24.9) DeviceID="rtp-core-rtr" DeviceDescription="RTP Core ASR" DeviceImportance="High" DeviceLocation="Raleigh" DeviceLayer="L3" DeviceContact="support@logzilla.net" DeviceRole="Core"
```

Furthermore, you would now have new "fields" (user tags) available in the UI, for example:

* DeviceImportance
* DeviceLocation
* DeviceRole

##### Screenshot: Available Fields

!["usertags_fields"](images/user-tag-fields.jpg)

