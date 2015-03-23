#!/bin/sh

# check parameters
if [ -z "$@" ]
then
	echo "USAGE: makefs.bat filename.(mod/xm/s3m)"
	exit 1
fi

# convert
bin/converter $@
[ $? -ne 0 ] && exit 1

# make filesystem
gbfs data.gbfs sample_bank.bin `echo $@ | sed -e "s/[^ ]\+/\0.bin/g"`

# append filesystem to rom
cat bin/example.bin > example.gba
cat data.gbfs >> example.gba
