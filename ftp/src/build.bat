del setup.exe 
del ftp_server.exe 
del ftp_client.exe 

gcc setup.c -o setup.exe 

C:\gcc\bin\g++.exe ftp_client.c -o ftp_client.exe
C:\gcc\bin\g++.exe ftp_server.c -o ftp_server.exe

rem C:\gcc\bin\g++.exe ftp_client.c -o ftp_client.exe
rem C:\gcc\bin\g++.exe ftp_server.c -o ftp_server.exe

pause


