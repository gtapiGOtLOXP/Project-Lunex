@echo off
title CS:Source Bunny Hop EXEC
color 0A
echo Perfect Bunny Hop Script Loaded!
echo Hold SPACE + A/D for infinite speed...
echo Press CTRL+C to stop.

:loop
REM Perfect 125ms timing (CS:S tickrate)
timeout /t 0 /nobreak >nul
if %time:~6,2%==25 (
    echo. [+] JUMP @ %time%
    echo ^| send ^!^!space {tap}
) else if %time:~6,2%==50 (
    echo. [+] JUMP @ %time%
    echo ^| send ^!^!space {tap}
) else if %time:~6,2%==75 (
    echo. [+] JUMP @ %time%
    echo ^| send ^!^!space {tap}
) else if %time:~6,2%==00 (
    echo. [+] JUMP @ %time%
    echo ^| send ^!^!space {tap}
)

REM Strafe assist
if %errorlevel%==0 (
    if 0%time:~-11,1%==5 (
        echo [+] STRAFE LEFT/RIGHT
    )
)

goto loop
