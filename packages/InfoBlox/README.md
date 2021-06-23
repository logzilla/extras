# InfoBlox DNS


# Installation

```
sudo su -
wget 
```

```
for rule in rules.d/*.yaml
do
  logzilla rules add $rule -f -R
done
```
```
logzilla rules reload
```

### Customers running LogZilla `v6.12` or lower must run the following commands:

```
# check to make sure you don't already have defined tags, if so, add them along with the new ones:
logziilla config | grep HIGH_CARDINALITY_TAGS
```
```
logzilla config HIGH_CARDINALITY_TAGS "Infoblox DNS Client IP, Infoblox DNS Client Query"
```
```
logzilla restart
```

```
wget '

for dashboard in dashboards/*.yaml
do
  logzilla dashboards import -I $dashboard
done
```


