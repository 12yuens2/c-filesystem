#SCRATCH_DIR=/cs/scratch/${USER}
echo "===START - Testing $1==="
make clean
cp $1/myfs.c .
cp $1/myfs.h .
make
#EXECFS=`./myfs`
#pushd ${SCRATCH_DIR}
# pwd
# if [ -d /cs/scratch/sy35/mnt ]; then
# rmdir /cs/scratch/sy35/mnt
# fi
# if [ ! -d /cs/scratch/sy35/mnt ]; then
# mkdir /cs/scratch/sy35/mnt
./myfs /cs/scratch/sy35/mnt
echo "--create an empty file--"
touch /cs/scratch/sy35/mnt/test.txt
ls -la /cs/scratch/sy35/mnt
echo "--stat--"
stat /cs/scratch/sy35/mnt/test.txt
sleep 3
echo "--remount the file system--"
fusermount -u /cs/scratch/sy35/mnt
sleep 3
./myfs /cs/scratch/sy35/mnt
ls -la /cs/scratch/sy35/mnt
echo "--chmod--"
chmod 660 /cs/scratch/sy35/mnt/test.txt
ls /cs/scratch/sy35/mnt
ls -la /cs/scratch/sy35/mnt
ls -la /cs/scratch/sy35/mnt/test.txt
echo "--mkdir--"
mkdir /cs/scratch/sy35/mnt/test
ls /cs/scratch/sy35/mnt
ls -la /cs/scratch/sy35/mnt
stat /cs/scratch/sy35/mnt/test.txt
mkdir /cs/scratch/sy35/mnt/test/test
ls /cs/scratch/sy35/mnt/test
ls -la /cs/scratch/sy35/mnt/test
echo "--chmod--"
chmod 600 /cs/scratch/sy35/mnt/test/test
ls -la /cs/scratch/sy35/mnt/test
ls -la /cs/scratch/sy35/mnt
echo "--write--"
echo "hello" > /cs/scratch/sy35/mnt/test.txt
cat /cs/scratch/sy35/mnt/test.txt
echo "hello" > /cs/scratch/sy35/mnt/test.txt
cat /cs/scratch/sy35/mnt/test.txt
echo "hello" >> /cs/scratch/sy35/mnt/test.txt
cat /cs/scratch/sy35/mnt/test.txt 
echo "hello2" > /cs/scratch/sy35/mnt/test/test.txt 
cat /cs/scratch/sy35/mnt/test/test.txt
tree /cs/scratch/sy35/mnt
echo "--truncate--"
truncate -s 2 /cs/scratch/sy35/mnt/test.txt
cat /cs/scratch/sy35/mnt/test.txt
echo "--delete file--"
rm -f /cs/scratch/sy35/mnt/test.txt 
ls -la /cs/scratch/sy35/mnt/
echo "--delete directory--"
rmdir /cs/scratch/sy35/mnt/test/test
ls -la /cs/scratch/sy35/mnt/test
echo "--create diectories--"
mkdir -p /cs/scratch/sy35/mnt/test/test/test/toast/boast/boat
ls -lR /cs/scratch/sy35/mnt
tree /cs/scratch/sy35/mnt
echo "===END - Testing $1==="
fusermount -u /cs/scratch/sy35/mnt
rm -f myfs.db myfs.log
# else
# echo "mount point already exists:"
# readlink -f /cs/scratch/sy35/mnt
# fi
# popd
