@echo off
cd /d "%~dp0"
start "" pythonw.exe "%~dp0monitor.py" --auto-connect
