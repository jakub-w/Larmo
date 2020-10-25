#!/usr/bin/sh

DIR="$(dirname $(realpath "$0"))"

docker run \
       --rm \
       --interactive \
       --tty \
       --name=larmo-player \
       --volume=$DIR/volume:/larmo \
       --volume=$DIR/alsa-conf.d:/etc/alsa/conf.d \
       --publish=5345:5345 \
       --device=/dev/snd:/dev/snd \
       larmo-player
