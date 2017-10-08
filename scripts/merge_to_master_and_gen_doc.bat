ECHO OFF
ECHO ***********************
ECHO This script updates the doxygen doc and merges 'develop' to 'master'
ECHO ***********************
ECHO ON
PAUSE

SET git=C:\Users\giuca\AppData\Local\Atlassian\SourceTree\git_local\bin\git.exe
%git% checkout master
PAUSE
%git% merge develop --commit --verbose
PAUSE
CALL generate_doc.bat
PAUSE
%git% add C:\projects\density\doc\html\* --force
PAUSE
%git% commit -m"Merging develop to master"
PAUSE
%git% push
PAUSE