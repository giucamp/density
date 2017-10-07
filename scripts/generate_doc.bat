
ECHO OFF

cd ..\doc\doxygen

ECHO deleting old files...
del ..\html\* /s /f /q >doxygen_out.txt 2>&1
del ..\latex\* /s /f /q >>doxygen_out.txt 2>>&1

ECHO running doxygen...
ECHO ------------------------- >>doxygen_out.txt 2>>&1
ECHO ------------------------- >>doxygen_out.txt 2>>&1
ECHO RUNNING DOXYGEN >>doxygen_out.txt 2>>&1
doxygen doxygen_config.txt >>doxygen_out.txt 2>>&1