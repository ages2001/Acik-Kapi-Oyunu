@echo off
cd /d "%~dp0"
copy /b "FP10R2.zip.001"+"FP10R2.zip.002"+"FP10R2.zip.003"+"FP10R2.zip.004"+"FP10R2.zip.005" "FP10R2.zip"
if errorlevel 1 exit /b 1
echo Archive reassembled.
