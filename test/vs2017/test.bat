REM SET CONFIGURATION=Release
REM SET PLATFORM=x64

IF "%CONFIGURATION%"=="Release" (
    ECHO Testing %PLATFORM% %CONFIGURATION%
    IF "%PLATFORM%"=="x64" (
        test\vs2017\x64\%CONFIGURATION%\density_tests.exe
    ) ELSE (
        test\vs2017\Win32\%CONFIGURATION%\density_tests.exe
    )    
)
