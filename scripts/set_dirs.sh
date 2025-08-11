#!/bin/sh

run=/var/run/balrog/
log=/var/log/balrog/

echo "$run"

sudo mkdir -p "$run"
sudo mkdir -p "$log"
sudo chown -R balrogd:balrogd "$run"
sudo chown -R balrogd:balrogd "$log"
sudo chmod 755 "$run" "$log"

echo "Listo"
