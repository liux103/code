del video_player.exe 

gcc\bin\gcc.exe -c video_player.c -o video_player.o ^
-I"gcc/include" ^
-I"gcc/x86_64-w64-mingw32/include" ^
-I"gcc/lib/gcc/x86_64-w64-mingw32/4.9.2/include" ^
-I"ffmpeg/include" ^
-I"sdl2/include/SDL2" ^
-I"sdl2/include" -m32

gcc\bin\gcc.exe video_player.o -o video_player.exe ^
-L"gcc/x86_64-w64-mingw32/lib32" ^
-L"ffmpeg/lib" ^
-L"sdl2/lib" ^
-static-libgcc -lmingw32 -lSDL2main -lSDL2 -lSDL2.dll ^
-lSDL2_test -lavcodec.dll -lavdevice.dll -lavfilter.dll ^
-lavformat.dll -lavutil.dll -lpostproc.dll -lswresample.dll ^
-lswscale.dll -m32

del video_player.o

pause


