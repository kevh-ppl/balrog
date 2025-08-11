#!/bin/sh

run=/var/run/balrog/
log=/var/log/balrog/

echo "$run"

if ! getent group balrogd > /dev/null 2>&1; then
    sudo addgroup --system balrogd
    echo "Grupo balrogd creado"
fi

if ! id -u balrogd > /dev/null 2>&1; then
    sudo useradd --system --no-create-home -g balrogd --shell /usr/sbin/nologin balrogd
    echo "User balrogd creado"
fi

sudo mkdir -p "$run"
sudo mkdir -p "$log"
sudo chown -R balrogd:balrogd "$run"
sudo chown -R balrogd:balrogd "$log"
sudo chmod 755 "$run" "$log"


#alias quitarbalrog='sudo make uninstall balrog && make clean && sudo rm -rf $HOME/.balrog && sudo -u balrogd rm -rf /var/log/balrog/* /var/run/balrog/* && echo "gay" && sudo pkill -9 -f balrog'
#
#alias ponerbalrog='make && sudo make install balrog'

echo "Listo"
