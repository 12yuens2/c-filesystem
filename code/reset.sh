./unmount.sh
make clean
make
./mount.sh
#touch /cs/scratch/sy35/mnt/a
#cat myfs.log
#fusermount -u /cs/scratch/sy35/mnt
#./myfs /cs/scratch/sy35/mnt
#ls /cs/scratch/sy35/mnt
#cat myfs.log
tail -f myfs.log
