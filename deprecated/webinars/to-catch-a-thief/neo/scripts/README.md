# Scripts

## getAP

This script is fired from a trigger (created in the UI).

Its purpose is to grab the Wireless Lan Controller's IP address as well as CapWap ID from the incoming Cisco ISE event and ssh to the WLC to get the Access Point the user is connected to.


## Requires PERL

Since this script uses perl modules, you will need to add them to the container in order for execute the script within that container.

Paste the following and you're good to go:

```
docker exec -it lz_celeryworker /bin/bash -c 'apt update; apt install liblwp-protocol-https-perl libnet-ssh2-perl libcrypt-ssleay-perl cpanminus build-essential -y'
docker exec -it lz_celeryworker /bin/bash -c 'cpanm Net::Telnet::Cisco Net::SSH2::Cisco HTTP::Request::Common LWP::UserAgent JSON'
```

## Containers are ephemeral!

Due to the nature of docker containers, restarting NEO (or your server) or if you upgrade NEO (basically, anything that stops the container), then files not distributed with the container are wiped.

We're currently developing a way for users to store special things such as perl modules that will be persistent. Until then, here's a simple function you can add to your `.bash_aliases` file:

```
neo() {
# note: I left 'perl' as a command line argument just in case you want to add other arguments
    case "$1" in
        addperl)
            docker exec -it lz_celeryworker /bin/bash -c 'apt update; apt install liblwp-protocol-https-perl libnet-ssh2-perl libcrypt-ssleay-perl cpanminus build-essential -y'
            docker exec -it lz_celeryworker /bin/bash -c 'cpanm Net::Telnet::Cisco Net::SSH2::Cisco HTTP::Request::Common LWP::UserAgent JSON'
            ;;
        *)
            echo $"Usage: neo addperl"
            return 1
    esac
}
```

## Another Option

Rewrite this example using something that doesn't require additional packages/modules.

If you do, please fork this repo and contribute back :)

