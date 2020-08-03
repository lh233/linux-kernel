@echo off
@title bat 交互执行git 每日提交命令
F:
cd F:\project\mygithub\linux-kernel
E:\publicsoftware\Git\bin\git.exe status .
E:\publicsoftware\Git\bin\git.exe add .
E:\publicsoftware\Git\bin\git.exe commit -m update
E:\publicsoftware\Git\bin\git.exe push origin master
pause