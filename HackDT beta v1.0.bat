@echo off
REM -----------------------------------------------
REM  By Mr.Bat
REM -----------------------------------------------
REM   MIT License
REM
REM   Copyright (c) 2026 Mr.Bat
REM
REM   Permission is hereby granted, free of charge, to any person obtaining a copy
REM   of this software and associated documentation files (the "Software"), to deal
REM   in the Software without restriction, including without limitation the rights
REM   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
REM   copies of the Software, and to permit persons to whom the Software is
REM   furnished to do so, subject to the following conditions:
REM
REM   The above copyright notice and this permission notice shall be included in all
REM   copies or substantial portions of the Software.
REM
REM   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
REM   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
REM   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
REM   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
REM   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
REM   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
REM   SOFTWARE.
REM -----------------------------------------------
@echo off
for /f %%a in ('echo prompt $E ^| cmd') do set "ESC=%%a"   
title HackDT Beta v1.0
if not exist "C:\HackDT" mkdir "C:\HackDT"

:Menu
cls
echo   ----------------
echo    Menu Principal
echo   ----------------
echo.
echo 1) Lista de ModMenus
echo 2) GitHub del proyecto
echo 3) %ESC%[31mLICENSE%ESC%[0m
echo 4) Salir
echo.


echo.
set /p opcion=Elige (1-4):
if "%opcion%"=="1" goto HackC
if "%opcion%"=="2" start "" "https://github.com/BatQuestions/HackDT" (
echo Abriendo navegador web...
)
if "%opcion%"=="3" goto LICENSE
if "%opcion%"=="4" exit
if %opcion% gtr 4 (
 echo %ESC%[31m --ERROR-- %ESC%[0mOpcion invalida, intenta de nuevo.
 )
if %opcion% lss 1 (
 echo %ESC%[31m --ERROR-- %ESC%[0mOpcion invalida, intenta de nuevo.
 )
pause 
goto Menu


:HackC
cls
echo   ------------
echo     ModMenus 
echo   ------------
echo.
if exist "C:\HackDT\BalMenu.exe" (
echo 1^) Abrir BalMenu
) else (
echo 1^) Descargar ModMenu para Balatro
)
if exist "C:\HackDT\UnderMenu.exe" (
echo 2^) Abrir UnderMenu
) else (
echo 2^) Descargar ModMenu para Undertale
)
if exist "C:\HackDT\ScritchyMenu.exe" (
echo 3^) Abrir ScritchyMenu
) else (
echo 3^) Descargar ModMenu para Scritchy Scratchy 
)
echo 4) Volver
echo.
set /p opcion=Elige (1-4):

if "%opcion%"=="1" (
if not exist "C:\HackDT\BalMenu.exe" (
curl -L -o "C:\HackDT\BalMenu.exe" "https://raw.githubusercontent.com/BatQuestions/HackDT/HackMenus/BalMenu/BalMenu.exe"
)
start "" "C:\HackDT\BalMenu.exe"
)

if "%opcion%"=="2" (
if not exist "C:\HackDT\UnderMenu.exe" (
curl -L -o "C:\HackDT\UnderMenu.exe" "https://raw.githubusercontent.com/BatQuestions/HackDT/HackMenus/UnderMenu/UnderMenu.exe"
)
start "" "C:\HackDT\UnderMenu.exe"
)

if "%opcion%"=="3" (
if not exist "C:\HackDT\ScritchyMenu.exe" (
 curl -L -o "C:\hackdt\ScritchyMenu.exe" "https://raw.githubusercontent.com/BatQuestions/HackDT/HackMenus/ScritchyMenu/ScritchyMenu.exe"
 )
 start "" "C:\HackDT\ScritchyMenu.exe
 )

if "%opcion%"=="4" goto Menu
if %opcion% gtr 4 (
 echo %ESC%[31m --ERROR-- %ESC%[0mOpcion invalida, intenta de nuevo.
 )
if %opcion% lss 1 (
 echo %ESC%[31m --ERROR-- %ESC%[0mOpcion invalida, intenta de nuevo.
 )
goto HackC



:LICENSE
cls
echo   ---------
echo    LICENSE
echo   ---------
echo.
echo MIT License
echo.
echo Copyright (c) 2026 Mr.Bat
echo.
echo Permission is hereby granted, free of charge, to any person obtaining a copy
echo of this software and associated documentation files (the "Software"), to deal
echo in the Software without restriction, including without limitation the rights
echo to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
echo copies of the Software, and to permit persons to whom the Software is
echo furnished to do so, subject to the following conditions:
echo. 
echo The above copyright notice and this permission notice shall be included in all
echo copies or substantial portions of the Software.
echo. 
echo THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
echo IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
echo FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
echo AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
echo LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
echo OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
echo SOFTWARE.
echo.
timeout /t 4 /nobreak
pause
goto Menu