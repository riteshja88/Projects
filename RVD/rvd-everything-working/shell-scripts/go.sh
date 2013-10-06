clear
sudo losetup /dev/loop0 /media/disk/disk1.img 
sudo losetup /dev/loop1 /media/disk/disk2.img
sudo make -C /lib/modules/`uname -r`/build M=`pwd` clean
echo "****************************************************************"
sudo make -C /lib/modules/`uname -r`/build M=`pwd` 
echo "****************************************************************"
sudo rmmod $1
echo "****************************************************************"
sudo insmod $1.ko
echo "****************************************************************"
dmesg > abc
echo "****************************************************************"
