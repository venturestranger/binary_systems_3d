@echo off
chcp 65001
title Setup
echo # Make sure you've run it as an administrator
pause

set /p dir=Project root directory (Where batch file is located): 
cd %dir%
echo # Now we are in %cd%

title MinGW preparation
echo # Here is an installer
echo # Set the MinGW into the root directory as it is!
start /wait libs\mingw\mingw-setup.exe
copy libs\addons\*.dll C:\Windows\System

title FreeGlut preparation
echo # Now is to set up freeglut libraries
copy libs\freeglut\include\GL\*.h C:\MinGW\include\GL
copy libs\freeglut\bin\freeglut.dll C:\Windows\System
copy libs\freeglut\lib\libfreeglut.a C:\MinGW\lib

title Assembling project
start /wait C:\MinGW\bin\g++.exe -IC:\MinGW\lib -Wall -g -std=c++14 -c app\main.cpp -o app\main.o
start /wait C:\MinGW\bin\g++.exe -o launch.exe app\main.o imgui\imgui.o imgui\imgui_demo.o imgui\imgui_tables.o imgui\imgui_draw.o imgui\imgui_widgets.o imgui\imgui_impl_glut.o imgui\imgui_impl_opengl2.o -O3 -lfreeglut -lglu32 -lopengl32 -lwinmm -lgdi32
title Assembling completed!

echo # Well done!
pause
