clear

while true
do
	echo "*********INTELLIGENT VIRTUAL STORAGE DEVICE**************"
	echo "1. LOAD INTELLIGENT VIRTUAL STORAGE DEVICE DRIVER"
	echo "2. UNLOAD INTELLIGENT VIRTUAL STORAGE DEVICE DRIVER"
	echo "3. FORMAT INTELLIGENT VIRTUAL STORAGE DEVICE"
	echo "4. MOUNT INTELLIGENT VIRTUAL STORAGE DEVICE"
	echo "5. UNMOUNT INTELLIGENT VIRTUAL STORAGE DEVICE"
	echo "6. OPEN MOUNTED DRIVE"	
	echo "7. SPEEDCHECK"	
	echo "8. EXIT"	
	echo "*********************************************************"
	echo -n "What do you want to do? "
	read choice
	case $choice in
		1) sh load.sh;echo "Load Driver command completed";;
		2) sh unload.sh;echo "Unload Driver command completed";;
		3) echo "Format Device command issued";sh format.sh;echo "Format Device comman -n d complete";;
		4) echo "Mount command issued";sh mount.sh;;
		5) sh unmount.sh;echo "Unmount command completed";;
		6) echo "Opening Mounted drive";nautilus /media/rvd;;
		7) sh request.sh;echo -n "Enter arguements to be passed: ";read a b c;
			#echo "Flushing Cache";sh clearcache.sh;
			sh request.sh $a $b $c ;rm a;echo "dd command complete";;
		8) echo "Bye!!!";exit;;
		*) echo "Invalid choice";;
	esac
	echo "\n\n"	
done
