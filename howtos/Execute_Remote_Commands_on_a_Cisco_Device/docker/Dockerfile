FROM ubuntu:20.04
 RUN apt update && apt install -y \
     liblwp-protocol-https-perl \
     libnet-ssh2-perl \
     libcrypt-ssleay-perl \
     cpanminus \
     build-essential
 RUN cpanm \
     Net::Telnet::Cisco \
     Net::SSH2::Cisco \
     HTTP::Request::Common \
     LWP::UserAgent JSON
