# LogZilla v5 Trigger Repository
The LZ5 trigger repo is here for us to share any scripts we've written for customers.

If you write any on your own, feel free to contribute them to this repository.

## Script Environment
All triggers passed to a script contain all of the matched message information as environment variables.
To manipulate any of the data, simply call that environment variable.

The following list of variables are passed into each script automatically:

    EVENT_COUNTER=<integer>
    EVENT_CISCO_MNEMONIC=<string>
    EVENT_STATUS=<integer>
    EVENT_ID=<integer>
    EVENT_SEVERITY=<integer>
    EVENT_FACILITY=<integer>
    EVENT_HOST=<string>
    EVENT_FIRST_OCCURRENCE=<float>
    EVENT_SNARE_ID=<integer>
    EVENT_PROGRAM=<string>
    EVENT_LAST_OCCURRENCE=<float>
    EVENT_MESSAGE=<string>
    EVENT_TRIGGER_ID=<integer>


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
3. Fill in the path to your new script, such as: `/var/lib/logzilla/scripts/myscript`

Any patterns matching this trigger will now be executed.

You may also want to modify the `Max Triggers Per Minute` and the `Max Triggers Per Second` throttle settings located at `Settings>System Settings>Triggers`

