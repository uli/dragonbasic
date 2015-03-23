@echo off

REM check parameters
if %1x==x goto usage

REM generate filename lists
set _infiles=
set _outfiles=
:start
if %1x==x goto end
echo %1
set _infiles=%_infiles% %1
set _outfiles=%_outfiles% %1.bin
shift
goto start
:end

REM convert
bin\converter %_infiles%
if not %ERRORLEVEL%==0 goto :EOF

REM make filesystem
gbfs data.gbfs sample_bank.bin %_outfiles%

REM append filesystem to rom
copy /B bin\example.bin+data.gbfs example.gba

goto :EOF

:usage
echo USAGE: makefs.bat filename.(mod/xm/s3m)
