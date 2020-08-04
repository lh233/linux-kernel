@echo off
@title bat 交互执行git 每日提交命令
F:
cd F:\project\mygithub\linux-kernel
git status .
git add .
@set /p option="please input y/n to commit??:" 

if "%option%" == "y" (
	 git commit
	 git push origin master
) 

if "%option%" == "n" (
	@echo "git commit nothing"
	pause
) 
  