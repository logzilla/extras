# LogZilla Rewrite Rules

# Release Version
LogZilla v5.75.1

# Format

LogZilla rewrite rules are stored in standard JSON format and contain types of:

* String(s) to match on
* Fields to use for rewrite

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

