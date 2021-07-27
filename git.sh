#!/bin/bash
PATH=/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin:/usr/local/sbin:~/bin
export PATH
project_path="/home/narwal/study/linux-kernel/"
project_date="`date +"update time:%Y-%m-%d %H:%M:%S"`"
cd $project_path
echo "update" > $project_path/update.log
git pull origin master
git add .
git commit -m 'update....'
git push origin master
echo "update end " >> $project_path/update.log
echo $project_date >> $project_path/update.log
