if [ -f "disk1.vhd" ]; then
    mv disk1.vhd disk1.img
fi

if [ ! -f "disk1.img" ]; then
    echo "error: no disk1.vhd, download it first!!!"
    exit -1
fi

if [ -f "disk2.vhd" ]; then
    mv disk2.vhd disk2.img
fi

if [ ! -f "disk2.img" ]; then
    echo "error: no disk2.vhd, download it first!!!"
    exit -1
fi

export DISK1_NAME=disk1.img

dd if=boot.bin of=$DISK1_NAME bs=512 conv=notrunc count=1

dd if=loader.bin of=$DISK1_NAME bs=512 conv=notrunc seek=1

dd if=kernel.elf of=$DISK1_NAME bs=512 conv=notrunc seek=100

export DISK2_NAME=disk2.img
export TARGET_PATH=mp
rm -rf $TARGET_PATH
mkdir $TARGET_PATH
sudo mount -o offset=$[128*512],rw $DISK2_NAME $TARGET_PATH
sudo cp -v *.elf $TARGET_PATH

sudo umount $TARGET_PATH
