# LogZilla v5 Trigger Repository

Feel free to contribute any triggers you've written.

## Script Environment
All triggers passed to a script contain all of the matched message information as environment variables.
To manipulate any of the data, simply call that environment variable.

The following list of variables are passed into each script automatically:
>Note: Some of the variables below are only available after LogZilla `v5.70.3`


	# EVENT_CISCO_MNEMONIC          =   <string>
	# EVENT_COUNTER                 =   <integer>
	# EVENT_FACILITY                =   <integer>
	# EVENT_FIRST_OCCURRENCE        =   <float>
	# EVENT_HOST                    =   <string>
	# EVENT_ID                      =   <int>
	# EVENT_LAST_OCCURRENCE         =   <float>
	# EVENT_MESSAGE                 =   <string>
	# EVENT_PROGRAM                 =   <string>
	# EVENT_SEVERITY                =   <integer>
	# EVENT_STATUS                  =   <integer>
	# EVENT_TRIGGER_AUTHOR          =   <string>
	# EVENT_TRIGGER_AUTHOR_EMAIL    =   <string>
	# EVENT_TRIGGER_ID              =   <integer>
	# EVENT_USER_TAGS               =   <integer>
	# TRIGGER_HITS_COUNT            =   <integer>

# Calling a script in LogZilla
>Note: the path where your scripts are stored must match the value set in LogZilla's `Settings>System Settings>Triggers` menu option.

From an SSH Console/Shell:

1. Create a new file at /var/lib/logzilla/scripts/myscript
2. Add the script contents and save the file
3. Run the following commands to change ownership and permissions on the script:


    chown logzilla:logzilla /var/lib/logzilla/scripts/myscript
    chmod 755 /var/lib/logzilla/scripts/myscript

Next, log into the LogZilla Web Interface and:

1. Create a new trigger from the trigger menu
2. Select the `execute script` option.
3. Select `myscript` from the dropdown menu

Any patterns matching this trigger will now call `myscript`

