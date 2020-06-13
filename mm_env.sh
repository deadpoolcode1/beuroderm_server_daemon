#!/bin/bash
cd /media/ilan/usbdisk/android-marshmallow
set -a
. build/envsetup.sh
. modular-tools build_image &
sleep 30 
kill %1
set +a
cd -
