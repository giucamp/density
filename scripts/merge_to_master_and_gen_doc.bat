SET git=C:\Users\giuca\AppData\Local\Atlassian\SourceTree\git_local\bin\git.exe
%git% checkout master
%git% merge develop --commit --verbose
CALL generate_doc.bat
%git% add C:\projects\density\doc\html\* --force
%git% commit -m"Merging develop to master"
PAUSE