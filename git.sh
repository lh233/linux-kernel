#!/bin/bash
PATH=/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin:/usr/local/sbin:~/bin
export PATH
project_path="/home/narwal/study/linux-kernel/"
project_date="`date +%y%m%d`"
cd $project_path
git pull origin master
git add $project_path*
git commit -m 'update....'
git push origin master
echo "update end " > $project_path/update.log
echo $project_date >> $project_path/update.log
