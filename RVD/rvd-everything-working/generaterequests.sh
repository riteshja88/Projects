if [ $# -le 2 ]
then
	echo "Syntax: generaterequests.sh SEEK_BYTES SIZE_BYTES"
	exit
fi
echo "#define SEEK $1" > file.c
echo "#define SIZE $2" >> file.c
cat generaterequests.c >> file.c
gcc file.c
sudo ./a.out
rm file.c

