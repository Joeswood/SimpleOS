if [ -f "disk1.vhd" ]; then
    mv disk1.vhd disk1.dmg
fi

if [ -f "disk2.vhd" ]; then
    mv disk2.vhd disk2.dmg
fi

export DISK1_NAME=disk1.dmg

dd if=boot.bin of=$DISK1_NAME bs=512 conv=notrunc count=1

dd if=loader.bin of=$DISK1_NAME bs=512 conv=notrunc seek=1

dd if=kernel.elf of=$DISK1_NAME bs=512 conv=notrunc seek=100

export DISK2_NAME=disk2.dmg
export TARGET_PATH=mp
rm $TARGET_PATH
hdiutil attach $DISK2_NAME -mountpoint $TARGET_PATH
cp -v *.elf $TARGET_PATH
hdiutil unmount $TARGET_PATH -verbose
