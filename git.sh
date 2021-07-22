#!/bin/bash
project_path="/home/narwal/study/linux-kernel/"
date="`date +%Y-%m-%d %H:%M:%S`"
cd $project_path
git add $project_path*
git commit
git push origin master
echo "update end " > $project_path/update.log
