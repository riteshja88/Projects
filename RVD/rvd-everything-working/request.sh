if [ $# -lt 3 ]
then
echo "Syntax: request.sh bs=BLOCK count=NUMBER_OF_BLOCKS skip=NUMBER_OF_BLOCKS_TO_SKIP"
exit
fi
#echo 1 > /proc/sys/vm/drop_caches
sudo dd if=/dev/rvd of=/dev/null $1 $2 $3

