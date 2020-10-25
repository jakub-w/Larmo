FROM ubuntu:latest

ENV TZ=Europe/Warsaw

RUN apt-get -y update
run DEBIAN_FRONTEND="noninteractive" apt-get -y install \
	libmpv1 \
	libgrpc++1

# Files this volume should contain:
# remote-player - an executable
# server.crt    - server's certificate
# server.key    - server's private key
# lrm.conf      - Larmo configuration file
VOLUME /larmo/

# This should be used for any ALSA configuration specific to the machine
# it's used on. Alsa on the host is required.
# For example if the card vith id of 1 should be used, a file with the
# following contents should be put in this directory:
#
# defaults.pcm.card 1
# defaults.ctl.card 1
#
VOLUME /etc/alsa/conf.d/

# The same port as the one defined in lrm.conf as grpc_port
EXPOSE 5345

WORKDIR larmo
ENTRYPOINT ./remote-player
# ENTRYPOINT bash
