#!/bin/bash
project_path="/home/narwal/study/linux-kernel/"
echo $(project_path) > /home/narwal/study/linux-kernel/1.log
cd $project_path
git add $project_path*
git commit -m 'update'
git push origin master
echo "update end " > $project_path/update.log
