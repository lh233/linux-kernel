@echo off
@title bat 交互执行git 提交命令
git status .
git add .
git commit -m update
git push origin master
