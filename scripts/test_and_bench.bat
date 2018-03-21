REM SET CONFIGURATION=Release
REM SET PLATFORM=x64

ECHO OFF

REM Setup based in the configuration
IF "%CONFIGURATION%"=="Debug" (
    SET QUEUE_TEST_CARD=1000
    SET BENCH=FALSE
)
IF "%CONFIGURATION%"=="DebugClang" (
    SET QUEUE_TEST_CARD=1000
    SET BENCH=FALSE
)
IF "%CONFIGURATION%"=="Release" (
    SET QUEUE_TEST_CARD=2000
    SET BENCH=TRUE
)
IF "%CONFIGURATION%"=="ReleaseClang" (
    SET QUEUE_TEST_CARD=2000
    SET BENCH=TRUE
)

REM Run test
ECHO Testing %PLATFORM% %CONFIGURATION%
IF "%PLATFORM%"=="x64" (
    test\vs2017\x64\%CONFIGURATION%\density_tests.exe -queue_tests_cardinality:%QUEUE_TEST_CARD%
) ELSE (
    test\vs2017\Win32\%CONFIGURATION%\density_tests.exe -queue_tests_cardinality:%QUEUE_TEST_CARD%
)

REM Run bench
IF "%BENCH%"=="TRUE" (
    ECHO Benchmarking %PLATFORM% %CONFIGURATION%
    IF "%PLATFORM%"=="x64" (
        bench\vs2017\x64\%CONFIGURATION%\density_bench.exe
    ) ELSE (
        bench\vs2017\Win32\%CONFIGURATION%\density_bench.exe
    )
)

REM PAUSE
