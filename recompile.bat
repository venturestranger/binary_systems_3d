title Recompiling
start /wait C:\MinGW\bin\g++.exe -IC:\MinGW\lib -Wall -g -std=c++14 -c app\main.cpp -o app\main.o
start /wait C:\MinGW\bin\g++.exe -o launch.exe app\main.o imgui\imgui.o imgui\imgui_demo.o imgui\imgui_tables.o imgui\imgui_draw.o imgui\imgui_widgets.o imgui\imgui_impl_glut.o imgui\imgui_impl_opengl2.o -O3 -lfreeglut -lglu32 -lopengl32 -lwinmm -lgdi32

echo # Well done!
pause
