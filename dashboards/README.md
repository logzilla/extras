# LogZilla Dashboards

# Format
LogZilla Dashboards are stored in standard JSON format.

1. Download the desired dashboard to your server
2. Once you have the raw `.json` file from github, simply import using the `logzilla dashboards import` command.


# Import/Export

1. Download the `raw` format of any of the dashboard files from Github. For example:
```
wget https://raw.githubusercontent.com/logzilla/extras/master/dashboards/General/dashboard-sample.json
```

## Import
```
logzilla import -I dashboard-sample.json
```

## Export

To export a dashboard:
```
logzilla export -O mydashboard.json
```

## Exporting from the UI

You may also export from the LogZilla UI using the menus. There are options for exporting single dashboards or all of them.



> The dashboards found here are either contributed by us or the community. They come with no warranty and should not be considered production quality unless you have personally tested and approved them in your environment.

