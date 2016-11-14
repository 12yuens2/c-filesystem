SCRATCH_DIR=/cs/scratch/${USER}
echo "===START - Testing $1==="
make clean
cp $1/myfs.c .
cp $1/myfs.h .
make
EXECFS=`readlink -f ./myfs`
pushd ${SCRATCH_DIR}
pwd
if [ -d mnttesting ]; then
rmdir mnttesting
fi
if [ ! -d mnttesting ]; then
mkdir mnttesting
${EXECFS} ./mnttesting
echo "--create an empty file--"
touch mnttesting/test.txt
ls -la mnttesting
echo "--stat--"
stat mnttesting/test.txt
sleep 3
echo "--remount the file system--"
fusermount -u mnttesting
${EXECFS} ./mnttesting
ls -la mnttesting
echo "--chmod--"
chmod 660 mnttesting/test.txt
ls mnttesting
ls -la mnttesting
ls -la mnttesting/test.txt
echo "--mkdir--"
mkdir mnttesting/test
ls mnttesting
ls -la mnttesting
stat mnttesting/test.txt
mkdir mnttesting/test/test
ls mnttesting/test
ls -la mnttesting/test
echo "--chmod--"
chmod 600 mnttesting/test/test
ls -la mnttesting/test
ls -la mnttesting
echo "--write--"
echo "hello" > mnttesting/test.txt
cat mnttesting/test.txt
echo "hello" > mnttesting/test.txt
cat mnttesting/test.txt
echo "hello" >> mnttesting/test.txt
cat mnttesting/test.txt 
echo "hello2" > mnttesting/test/test.txt 
cat mnttesting/test/test.txt
tree mnttesting
echo "--truncate--"
truncate -s 2 mnttesting/test.txt
cat mnttesting/test.txt
echo "--delete file--"
rm -f mnttesting/test.txt 
ls -la mnttesting/
echo "--delete directory--"
rmdir mnttesting/test/test
ls -la mnttesting/test
echo "--create diectories--"
mkdir -p mnttesting/test/test/test/toast/boast/boat
ls -lR mnttesting
tree mnttesting
echo "===END - Testing $1==="
fusermount -u mnttesting
rm -f myfs.db myfs.log
else
echo "mount point already exists:"
readlink -f mnttesting
fi
popd
