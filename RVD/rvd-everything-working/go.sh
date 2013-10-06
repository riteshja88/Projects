clear
#sudo mount -t ext3 /dev/sdb1 /media/hd
#sudo mount -t ext3 /dev/sdc1 /media/ssd
#sudo losetup /dev/loop0 /media/ssd/disk1
#sudo losetup /dev/loop1 /media/hd/disk2.img
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
