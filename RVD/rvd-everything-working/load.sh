sudo rmmod rvd
sudo make -C /lib/modules/`uname -r`/build M=`pwd` clean > a
sudo make -C /lib/modules/`uname -r`/build M=`pwd` >a
sudo rm a
sudo insmod rvd.ko reinitialise=$1
