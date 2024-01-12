#!/bin/bash
rm -rf out_small out_all
mkdir out_small out_all
for i in images images_small
do
    ./build/server_img -q 100 -w 1 -p FIFO 2123 > $i.txt & ./client -a 30 -I ./$i -n 1000 2123
    for j in IMG_REGISTER IMG_ROT90CLKW IMG_BLUR IMG_SHARPEN IMG_VERTEDGES IMG_HORIZEDGES
    do
	grep $j $i.txt > $i\_$j\_ops.csv
    done
done


