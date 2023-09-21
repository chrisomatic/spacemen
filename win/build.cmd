@echo off

mkdir .\build
pushd .\build

rmdir /Q /S src
mkdir src\core\shaders src\core\fonts src\img src\effects src\themes

set srcdir=..\..\src

xcopy %srcdir%\img src\img
xcopy %srcdir%\effects src\effects
xcopy %srcdir%\themes src\themes

xcopy %srcdir%\core\shaders src\core\shaders
xcopy %srcdir%\core\fonts src\core\fonts


set srcfiles=%srcdir%\core\gfx.c %srcdir%\core\shader.c %srcdir%\core\timer.c %srcdir%\core\math2d.c %srcdir%\core\window.c %srcdir%\core\imgui.c %srcdir%\core\glist.c %srcdir%\core\socket.c %srcdir%\core\particles.c %srcdir%\core\text_list.c %srcdir%\player.c %srcdir%\net.c %srcdir%\settings.c %srcdir%\projectile.c %srcdir%\effects.c %srcdir%\editor.c %srcdir%\main.c
set opts=/O2 /D "_CRT_SECURE_NO_WARNINGS" /nologo
set includes=/I..\include /I%srcdir% /I%srcdir%\core /I..\dlls
set libs="OpenGL32.lib" "GLu32.lib" "glfw3_mt.lib" "glew32.lib" "kernel32.lib" "user32.lib" "gdi32.lib" "winspool.lib" "comdlg32.lib" "advapi32.lib" "shell32.lib" "ole32.lib" "oleaut32.lib" "uuid.lib" "odbc32.lib" "odbccp32.lib"

cl %opts% %includes% %srcfiles% /link /LIBPATH:..\lib /NODEFAULTLIB:MSVCRT %libs% /OUT:Spacemen.exe 
popd