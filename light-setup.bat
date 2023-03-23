@echo off
title Light Setup
chcp 65001
echo # Make sure you've run it as an administrator
pause

set /p dir=Project root directory (Where batch file is located): 
cd %dir%
copy libs\addons\*.dll C:\Windows\System
copy libs\freeglut\bin\freeglut.dll C:\Windows\System

echo # Well done!
pause
