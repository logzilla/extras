# LogZilla Dashboards

# Format
LogZilla Dashboards are stored in standard JSON format. As of `v5.72.1`, dashboards may be imported and exported from the command line.

1. Download the desired dashboard to your server
2. Once you have the raw `.json` file from github, simply import using the `lz5dashboards import` command.


# Import
* As an example, let's install the `General` dashboard.

```
wget https://raw.githubusercontent.com/logzilla/extras/master/dashboards/General/dashboard-general.json
```

* import using the lz5dashboards command.

```
~logzilla/src/bin/lz5dashboards import -I dashboard-general.json
```

## Export
You can export your current dashboards as well, for example:

```
~logzilla/src/bin/lz5dashboards export -O mydashboards.json
```

# Import/Export from the UI

We're also implementing this feature so that users can just do all of this directly from the UI.
The expected release version for this feature is `v5.78` 

> The dashboards found here are either contributed by us or the community. They come with no warranty and should not be considered production quality unless you have personally tested and approved them in your environment.

