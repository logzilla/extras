# SonicWall Rules

Be sure to load the associated rules for this dashboard located in ../../rules.d/untested/SonicWall/

[LINK](../../../dashboards/SonicWall)

# Or do this from your LogZilla Server:

```
sudo su -
wget 'https://raw.githubusercontent.com/logzilla/extras/master/rules.d/untested/SonicWall/500-sonicwall.yaml'
wget 'https://raw.githubusercontent.com/logzilla/extras/master/rules.d/untested/SonicWall/501-sonicwall-normalize.yaml'
wget 'https://raw.githubusercontent.com/logzilla/extras/master/dashboards/SonicWall/dashboard-sonicwall.yaml'
logzilla rules add 500-sonicwall.yaml
logzilla rules add 501-sonicwall-normalize.yaml
logzilla dashboards import -I dashboard-sonicwall.yaml
```

##### Sample

![Sonicwall Dashboard](../../../dashboards/SonicWall/sonicwall-dashboard-sample.png)
