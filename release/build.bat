del rtmp_video.exe setup.exe ftp_server.exe ftp_client.exe rtmp_audio.exe play_video.exe

C:\gcc\bin\gcc setup.c -o setup.exe
C:\gcc\bin\g++.exe ftp_client.c -o ftp_client.exe
C:\gcc\bin\g++.exe ftp_server.c -o ftp_server.exe

D:\Dev-Cpp\MinGW64\bin\gcc.exe -c rtmp_video.c -o rtmp_video.o ^
-I"D:/Dev-Cpp/MinGW64/include" ^
-I"D:/Dev-Cpp/MinGW64/x86_64-w64-mingw32/include" ^
-I"D:/Dev-Cpp/MinGW64/lib/gcc/x86_64-w64-mingw32/4.9.2/include" ^
-I"C:/Users/Administrator/Desktop/video/ffmpeg/include" ^
-I"C:/Users/Administrator/Desktop/video/sdl2/include" -m32   

D:\Dev-Cpp\MinGW64\bin\gcc.exe rtmp_video.o -o rtmp_video.exe ^
-L"D:/Dev-Cpp/MinGW64/x86_64-w64-mingw32/lib32" ^
-L"C:/Users/Administrator/Desktop/video/ffmpeg/lib" ^
-L"C:/Users/Administrator/Desktop/video/sdl2/lib" ^
-static-libgcc -lmingw32 -lSDL2main -lSDL2 -lSDL2.dll ^
-lSDL2_test -lavcodec.dll -lavdevice.dll -lavfilter.dll ^
-lavformat.dll -lavutil.dll -lpostproc.dll -lswresample.dll ^
-lswscale.dll -m32

D:\Dev-Cpp\MinGW64\bin\gcc.exe -c rtmp_audio.c -o rtmp_audio.o ^
-I"D:/Dev-Cpp/MinGW64/include" ^
-I"D:/Dev-Cpp/MinGW64/x86_64-w64-mingw32/include" ^
-I"D:/Dev-Cpp/MinGW64/lib/gcc/x86_64-w64-mingw32/4.9.2/include" ^
-I"C:/Users/Administrator/Desktop/video/ffmpeg/include" ^
-I"C:/Users/Administrator/Desktop/video/sdl2/include" -m32   

D:\Dev-Cpp\MinGW64\bin\gcc.exe rtmp_audio.o -o rtmp_audio.exe ^
-L"D:/Dev-Cpp/MinGW64/x86_64-w64-mingw32/lib32" ^
-L"C:/Users/Administrator/Desktop/video/ffmpeg/lib" ^
-L"C:/Users/Administrator/Desktop/video/sdl2/lib" ^
-static-libgcc -lmingw32 -lSDL2main -lSDL2 -lSDL2.dll ^
-lSDL2_test -lavcodec.dll -lavdevice.dll -lavfilter.dll ^
-lavformat.dll -lavutil.dll -lpostproc.dll -lswresample.dll ^
-lswscale.dll -m32

D:\Dev-Cpp\MinGW64\bin\gcc.exe -c play_video.c -o play_video.o ^
-I"D:/Dev-Cpp/MinGW64/include" ^
-I"D:/Dev-Cpp/MinGW64/x86_64-w64-mingw32/include" ^
-I"D:/Dev-Cpp/MinGW64/lib/gcc/x86_64-w64-mingw32/4.9.2/include" ^
-I"C:/Users/Administrator/Desktop/video/ffmpeg/include" ^
-I"C:/Users/Administrator/Desktop/video/sdl2/include" -m32   

D:\Dev-Cpp\MinGW64\bin\gcc.exe play_video.o -o play_video.exe ^
-L"D:/Dev-Cpp/MinGW64/x86_64-w64-mingw32/lib32" ^
-L"C:/Users/Administrator/Desktop/video/ffmpeg/lib" ^
-L"C:/Users/Administrator/Desktop/video/sdl2/lib" ^
-static-libgcc -lmingw32 -lSDL2main -lSDL2 -lSDL2.dll ^
-lSDL2_test -lavcodec.dll -lavdevice.dll -lavfilter.dll ^
-lavformat.dll -lavutil.dll -lpostproc.dll -lswresample.dll ^
-lswscale.dll -m32

del rtmp_video.o
del rtmp_audio.o
del play_video.o

pause


