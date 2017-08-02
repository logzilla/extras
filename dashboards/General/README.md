# LogZilla Sample Dashboard

This dashboard provides a General overview for your incoming event streams. Widgets included:

* EPD: All Events
* EPS: All Events
* Unknown Events
* Actionable Events
* Latest Unread Notifications
* Top Hosts
* Recent Error Messages
* Failed Messages
* Non Actionable EPS
* Most Recent Event Sources

# Import/Export

1. Download the `raw` format of any of the dashboard files from Github. For example:
```
wget https://raw.githubusercontent.com/logzilla/extras/master/dashboards/General/dashboard-sample.json
```

## Import
```
/home/logzilla/src/bin/lz5dashboards import -I dashboard-sample.json
```

## Export

To export a dashboard:
```
/home/logzilla/src/bin/lz5dashboards export -O mydashboard.json
```

## Exporting from the UI

You may also export from the LogZilla UI using the menus. There are options for exporting single dashboards or all of them.

