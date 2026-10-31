@echo off
setlocal
title Windows GPU diagnostics setup
powershell.exe -NoProfile -File "%~dp0tools\start-capture.ps1"
if errorlevel 1 echo Setup stopped. Read the message above or docs\CAPTURE-SETUP.en.md.
pause
