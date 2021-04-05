
# LogZilla with Letsencrypt

If you want to use letsencrypt for a public SSL, simply add a redirect container using the following steps:


1. Make sure you have a DNS name already pointing to your logzilla server (or the ip where this ssl redirect will be installed).

Edit `data/nginx/app.conf` and change the following lines to match the dns for your server:

```
# line 3:
server_name logzilla.example.com;

# line 13:
server_name logzilla.example.com;
 
# lines 25 and 26:

ssl_certificate /etc/letsencrypt/live/logzilla.example.com/fullchain.pem;
ssl_certificate_key /etc/letsencrypt/live/logzilla.example.com/privkey.pem;

```

Edit `init-letsencrypt.sh` and change :

```

# line 8
domains=(logzilla.example.com)

# line 11
email="user@foo.com"

```

2. Set LogZilla to its default ports instead of port 80/443:

```
logzilla config HTTP_PORT_MAPPING tcp/80:3280,tcp/443:32443
```

3. Restart logzilla:

```
logzilla restart
```

4. Make sure you have [docker compose](https://www.digitalocean.com/community/tutorials/how-to-install-and-use-docker-compose-on-ubuntu-20-04) installed

5. then run:

```
./init-letsencrypt.sh
```

That's it. Any incoming requests on port 80 will auto-redirect to port 443, port 443 requests will redirect to the internal LogZilla container.






