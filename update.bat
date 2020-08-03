@echo off
@title bat 交互执行git 提交命令
F:
cd F:\project\mygithub\linux-kernel
git status .
git add .
git commit -m update
git push origin master
pause