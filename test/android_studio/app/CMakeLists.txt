# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.

cmake_minimum_required(VERSION 3.4.1)


include_directories("../..")
include_directories("../../../")

# Creates and names a library, sets it as either STATIC
# or SHARED, and provides the relative paths to its source code.
# You can define multiple libraries, and CMake builds them for you.
# Gradle automatically packages shared libraries with your APK.

add_library( # Sets the name of the library.
             native-lib

             # Sets the library as a shared library.
             SHARED

            ../../examples/concurrent_heterogeneous_queue_examples.cpp
            ../../examples/conc_func_queue_examples.cpp
            ../../examples/func_queue_examples.cpp
            ../../examples/heterogeneous_queue_examples.cpp
            ../../examples/lf_func_queue_examples.cpp
            ../../examples/lf_heterogeneous_queue_examples.cpp
            ../../examples/misc_examples.cpp
            ../../examples/lifo_examples.cpp
            ../../examples/sp_func_queue_examples.cpp
            ../../examples/sp_heterogeneous_queue_examples.cpp
            ../../tests/generic_tests/conc_heter_queue_generic_tests.cpp
            ../../tests/generic_tests/heter_queue_generic_tests.cpp
            ../../tests/generic_tests/lf_heter_queue_generic_tests_relaxed.cpp
            ../../tests/generic_tests/lf_heter_queue_generic_tests_seqcst.cpp
            ../../tests/generic_tests/queue_generic_tests.cpp
            ../../tests/generic_tests/sp_heter_queue_generic_tests_seqcst.cpp
            ../../tests/concurrent_heterogeneous_queue_basic_tests.cpp
            ../../tests/heterogeneous_queue_basic_tests.cpp
            ../../tests/lf_heterogeneous_queue_basic_tests.cpp
            ../../tests/load_unload_tests.cpp
            ../../tests/sp_heterogeneous_queue_basic_tests.cpp
            ../../tests/lifo_tests.cpp
            ../../tests/user_data_stack.cpp
            ../../test_framework/density_test_common.cpp
            ../../test_framework/dynamic_type.cpp
            ../../test_framework/exception_tests.cpp
            ../../test_framework/histogram.cpp
            ../../test_framework/line_updater_stream_adapter.cpp
            ../../test_framework/progress.cpp
            ../../test_framework/shared_block_registry.cpp
            ../../test_framework/test_allocators.cpp
            ../../test_framework/test_objects.cpp
            ../../test_framework/threading_extensions.cpp
            ../../test_settings.cpp
            ../../main.cpp

             # Provides a relative path to your source file(s).
             src/main/cpp/native-lib.cpp )

# Searches for a specified prebuilt library and stores the path as a
# variable. Because CMake includes system libraries in the search path by
# default, you only need to specify the name of the public NDK library
# you want to add. CMake verifies that the library exists before
# completing its build.

find_library( # Sets the name of the path variable.
              log-lib

              # Specifies the name of the NDK library that
              # you want CMake to locate.
              log )

# Specifies libraries CMake should link to your target library. You
# can link multiple libraries, such as libraries you define in this
# build script, prebuilt third-party libraries, or system libraries.

target_link_libraries( # Specifies the target library.
                       native-lib

                       # Links the target library to the log library
                       # included in the NDK.
                       ${log-lib} )