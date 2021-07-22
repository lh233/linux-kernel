#!/bin/bash
echo "update" > /home/narwal/study/1.txt
/usr/bin/git add .
/usr/bin/git commit -m 'update'
/usr/bin/git push origin master
