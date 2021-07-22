#!/bin/bash
echo "hello world" > 1.txt
git add .
git commit -m 'update'
git config user.name 'lh233'
git config user.email '490095224@qq.com'
git push origin master

