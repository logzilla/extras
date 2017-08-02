# LogZilla Dashboards

# Format
LogZilla Dashboards are stored in standard JSON format. As of `v5.72.1`, dashboards may be imported and exported from the command line.

1. Download the desired dashboard to your server
2. Once you have the raw `.json` file from github, simply import using the `lz5dashboards import` command.


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



> The dashboards found here are either contributed by us or the community. They come with no warranty and should not be considered production quality unless you have personally tested and approved them in your environment.

