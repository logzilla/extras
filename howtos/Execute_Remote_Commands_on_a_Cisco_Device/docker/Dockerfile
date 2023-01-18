FROM ubuntu:20.04
 RUN apt update && apt install -y \
     liblwp-protocol-https-perl \
     libnet-ssh2-perl \
     libcrypt-ssleay-perl \
     cpanminus \
     build-essential \
     locales
 RUN cpanm \
     Net::Telnet::Cisco \
     Net::SSH2::Cisco \
     HTTP::Request::Common \
     LWP::UserAgent JSON
 RUN sed -i -e 's/# en_US.UTF-8 UTF-8/en_US.UTF-8 UTF-8/' /etc/locale.gen && \
    locale-gen
 ENV LC_ALL en_US.UTF-8
 ENV LANG en_US.UTF-8
 ENV LANGUAGE en_US.UTF-9
