@echo off
set PATH=C:\msys64\mingw64\bin;%PATH%
echo 正在运行装甲板识别...
"%~dp0build\armor_detector.exe" "%~dp0..\微信图片_20260528212437_2_919.png"
echo.
echo 完成！结果保存在 build\result.jpg
pause
start "%~dp0build\result.jpg"
