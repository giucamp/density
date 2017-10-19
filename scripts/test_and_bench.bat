SET CONFIGURATION=Release
SET PLATFORM=x64

ECHO Testing %PLATFORM% %CONFIGURATION%
IF "%PLATFORM%"=="x64" (
    test\vs2017\x64\%CONFIGURATION%\density_tests.exe
) ELSE (
    test\vs2017\Win32\%CONFIGURATION%\density_tests.exe
)

IF "%CONFIGURATION%"=="Release" (
    ECHO Benchmarking %PLATFORM% %CONFIGURATION%
    IF "%PLATFORM%"=="x64" (
        test\vs2017\x64\%CONFIGURATION%\density_bench.exe -source:"bench\vs2017"
    ) ELSE (
        test\vs2017\Win32\%CONFIGURATION%\density_bench.exe -source:"bench\vs2017"
    )
)

PAUSE
