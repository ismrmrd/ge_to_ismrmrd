
#!/bin/sh

# -------------------------------------------------------------------------------
#
# Taken from ISMRM 2019 class demo, at:
#
#    https://github.com/roopchansinghv/ISMRM2019_demo/blob/master/setup_ge_demo.sh
#
# -------------------------------------------------------------------------------

ismrmrd2png()
{
    fname=$1
    [ "$fname" == "" ] && fname="out.h5"

    if [ ! -f "$fname" ]; then
	echo "$0: $fname not found"
	return
    fi

    group_name=$( h5ls "$fname" | cut -d ' ' -f 1,2 | sed -e 's@\\@@' )

    imgsets=$( h5ls "$fname"/"$group_name" | cut -d ' ' -f 1 )

    for imgset in $imgsets; do

	line=$( h5ls $fname/"$group_name"/"$imgset" | egrep '^data' )
	nslice=$( echo $line | sed -e 's@.*{@@' -e 's@/.*@@')

	for (( slice=0; slice<$nslice; ++slice )); do
	    h5topng -x $slice -r -d "$group_name"/$imgset/data -o temp.png out.h5
	    convert temp.png -transpose ${imgset}_$( printf "%02d" $slice ).png
	done
	rm -f temp.png
    done
}

