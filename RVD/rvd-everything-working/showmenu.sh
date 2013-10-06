clear

while true
do
	echo "*********INTELLIGENT VIRTUAL STORAGE DEVICE**************"
	echo "1. LOAD INTELLIGENT VIRTUAL STORAGE DEVICE DRIVER"
	echo "2. LOAD INTELLIGENT VIRTUAL STORAGE DEVICE DRIVER WITH TABLE REINITIALISED"	
	echo "3. UNLOAD INTELLIGENT VIRTUAL STORAGE DEVICE DRIVER"
	echo "4. FORMAT INTELLIGENT VIRTUAL STORAGE DEVICE"
	echo "5. MOUNT INTELLIGENT VIRTUAL STORAGE DEVICE"
	echo "6. UNMOUNT INTELLIGENT VIRTUAL STORAGE DEVICE"
	echo "7. OPEN MOUNTED DRIVE"	
	echo "8. SPEEDCHECK"	
	echo "9. EXIT"	
	echo "*********************************************************"
	echo -n "What do you want to do?"
	read choice
	case $choice in
		1) sh load.sh 0;echo "Load Driver command completed";;
		2) sh load.sh 1;echo "Load Driver command completed with tables reinitialised.";;
		3) sh unload.sh;echo "Unload Driver command completed";;
		4) echo "Format Device command issued";sh format.sh;echo "Format Device command complete";;
		5) echo "Mount command issued";sh mount.sh;;
		6) sh unmount.sh;echo "Unmount command completed";;
		7) echo "Opening Mounted drive";sudo nautilus /media/rvd;;
		8) sh request.sh;echo -n "Enter arguments to be passed: ";read a b c;
			#echo "Flushing Cache";sh clearcache.sh;
			sh request.sh $a $b $c ;rm a;echo "dd command complete";;
		9) echo "Bye!!!";exit;;
		*) echo "Invalid choice";;
	esac
	echo "\n\n"	
done
