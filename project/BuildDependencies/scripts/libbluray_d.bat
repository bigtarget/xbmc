@ECHO OFF

SET LOC_PATH=%CD%
SET FILES=%LOC_PATH%\libbluray_d.txt

CALL dlextract.bat libbluray %FILES%

cd %TMP_PATH%

xcopy libbluray-8e5d24-win32-1\include\libbluray "%CUR_PATH%\include\libbluray" /E /Q /I /Y
copy libbluray-8e5d24-win32-1\bin\libbluray.dll "%XBMC_PATH%\system\players\dvdplayer\" /Y

cd %LOC_PATH%
