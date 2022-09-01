#!/bin/sh

MYDIR=`pwd`
RB_DIR=~/Development/rockbox
BUILD_DIR=$RB_DIR/build-ipodnano3g
MK6G_DIR=$RB_DIR/rbutil/mks5lboot
FILE_DFU_INST=dualboot-installer-ipodnano3g.dfu
FILE_DFU_UNINST=dualboot-uninstaller-ipodnano3g.dfu
FILE_BL=bootloader-ipodnano3g.ipod

# cd ~/Rockbox/git/rockbox/build/
echo "------------------------ build bootloader ---------------------"
cd $BUILD_DIR
make clean
make -j
echo "--"
ls -l *.bin
echo

echo "------------------------ build dualboot -----------------------"
#cd ~/Rockbox/git/rockbox/rbutil/mks5lboot/dualboot/;
cd $MK6G_DIR/dualboot/
make clean
echo "--"
make
echo

echo "------------------------ build mks5lboot -----------------------"
# cd ~/Rockbox/git/rockbox/rbutil/mks5lboot/;
cd $MK6G_DIR
make clean
echo "--"
make
echo "--"
ls -l ./dualboot/*bin ./dualboot/build/
echo

echo "------------------------ build DFU files ----------------------"
# cd ~/Rockbox/git/rockbox/build/
cd $BUILD_DIR
#  rm dualboot-uninstaller-ipodnano3g.dfu dualboot-installer-ipodnano3g.dfu
rm $FILE_DFU_INST $FILE_DFU_UNINST
# ./mkdfu ../../build/bootloader-ipodnano3g.ipod uninstaller-ipodnano3g.dfu -u
# ../rbutil/mks5lboot/mks5lboot ./bootloader-ipodnano3g.ipod dualboot-uninstaller-ipodnano3g.dfu -u
# ../rbutil/mks5lboot/mks5lboot --uninstall ./bootloader-ipodnano3g.ipod dualboot-uninstaller-ipodnano3g.dfu
# ../rbutil/mks5lboot/mks5lboot --mkdfu ./bootloader-ipodnano3g.ipod dualboot-uninstaller-ipodnano3g.dfu --uninstall
$MK6G_DIR/mks5lboot --mkdfu-inst $FILE_BL $FILE_DFU_INST
echo "--"
# ./mkdfu ../../build/bootloader-ipodnano3g.ipod installer-ipodnano3g.dfu
# ../rbutil/mks5lboot/mks5lboot ./bootloader-ipodnano3g.ipod dualboot-installer-ipodnano3g.dfu
#../rbutil/mks5lboot/mks5lboot --mkdfu ./bootloader-ipodnano3g.ipod dualboot-installer-ipodnano3g.dfu
$MK6G_DIR/mks5lboot --mkdfu-uninst ipodnano3g $FILE_DFU_UNINST
echo "--"
ls -l $FILE_DFU_INST $FILE_DFU_UNINST
echo

cd $MYDIR
cp ../rbutil/mks5lboot/mks5lboot .
