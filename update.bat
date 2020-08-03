@echo off
@title bat 交互执行git 提交命令
git status .
git add .
git commit -m "%date:~0,4%年%date:~5,2%月%date:~8,2%日 update"
git log .
