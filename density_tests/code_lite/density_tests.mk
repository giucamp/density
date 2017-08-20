##
## Auto Generated makefile by CodeLite IDE
## any manual changes will be erased      
##
## Release
ProjectName            :=density_tests
ConfigurationName      :=Release
WorkspacePath          :=/home/giuseppe/Projects/density/density_tests/code_lite
ProjectPath            :=/home/giuseppe/Projects/density/density_tests/code_lite
IntermediateDirectory  :=./Release
OutDir                 := $(IntermediateDirectory)
CurrentFileName        :=
CurrentFilePath        :=
CurrentFileFullPath    :=
User                   :=Giuseppe
Date                   :=20/08/17
CodeLitePath           :=/home/giuseppe/.codelite
LinkerName             :=/usr/bin/g++
SharedObjectLinkerName :=/usr/bin/g++ -shared -fPIC
ObjectSuffix           :=.o
DependSuffix           :=.o.d
PreprocessSuffix       :=.i
DebugSwitch            :=-g 
IncludeSwitch          :=-I
LibrarySwitch          :=-l
OutputSwitch           :=-o 
LibraryPathSwitch      :=-L
PreprocessorSwitch     :=-D
SourceSwitch           :=-c 
OutputFile             :=$(IntermediateDirectory)/$(ProjectName)
Preprocessors          :=$(PreprocessorSwitch)NDEBUG 
ObjectSwitch           :=-o 
ArchiveOutputSwitch    := 
PreprocessOnlySwitch   :=-E
ObjectsFileList        :="density_tests.txt"
PCHCompileFlags        :=
MakeDirCommand         :=mkdir -p
LinkOptions            :=  -pthread
IncludePath            :=  $(IncludeSwitch). $(IncludeSwitch). $(IncludeSwitch)$(ProjectPath)/.. $(IncludeSwitch)$(ProjectPath)/../../../density 
IncludePCH             := 
RcIncludePath          := 
Libs                   := 
ArLibs                 :=  
LibPath                := $(LibraryPathSwitch). 

##
## Common variables
## AR, CXX, CC, AS, CXXFLAGS and CFLAGS can be overriden using an environment variables
##
AR       := /usr/bin/ar rcu
CXX      := /usr/bin/g++
CC       := /usr/bin/gcc
CXXFLAGS :=  -O3 -std=c++11 -Wall $(Preprocessors)
CFLAGS   :=  -O2 -Wall $(Preprocessors)
ASFLAGS  := 
AS       := /usr/bin/as


##
## User defined environment variables
##
CodeLiteDir:=/usr/share/codelite
Objects0=$(IntermediateDirectory)/up_tests_nonblocking_heterogeneous_queue_basic_tests.cpp$(ObjectSuffix) $(IntermediateDirectory)/up_tests_concurrent_heterogeneous_queue_basic_tests.cpp$(ObjectSuffix) $(IntermediateDirectory)/up_tests_load_unload_tests.cpp$(ObjectSuffix) $(IntermediateDirectory)/up_tests_heterogeneous_queue_basic_tests.cpp$(ObjectSuffix) $(IntermediateDirectory)/up_main.cpp$(ObjectSuffix) $(IntermediateDirectory)/up_test_framework_progress.cpp$(ObjectSuffix) $(IntermediateDirectory)/up_test_framework_histogram.cpp$(ObjectSuffix) $(IntermediateDirectory)/up_test_framework_line_updater_stream_adapter.cpp$(ObjectSuffix) $(IntermediateDirectory)/up_test_framework_dynamic_type.cpp$(ObjectSuffix) $(IntermediateDirectory)/up_test_framework_test_objects.cpp$(ObjectSuffix) \
	$(IntermediateDirectory)/up_test_framework_threading_extensions.cpp$(ObjectSuffix) $(IntermediateDirectory)/up_test_framework_density_test_common.cpp$(ObjectSuffix) $(IntermediateDirectory)/up_test_framework_shared_block_registry.cpp$(ObjectSuffix) $(IntermediateDirectory)/up_test_framework_exception_tests.cpp$(ObjectSuffix) $(IntermediateDirectory)/up_up_examples_concurrent_heterogeneous_queue_examples.cpp$(ObjectSuffix) $(IntermediateDirectory)/up_up_examples_heterogeneous_queue_examples.cpp$(ObjectSuffix) $(IntermediateDirectory)/up_up_examples_nonblocking_heterogeneous_queue_examples.cpp$(ObjectSuffix) $(IntermediateDirectory)/up_tests_generic_tests_nb_heter_queue_generic_tests_relaxed.cpp$(ObjectSuffix) $(IntermediateDirectory)/up_tests_generic_tests_nb_heter_queue_generic_tests_seqcst.cpp$(ObjectSuffix) $(IntermediateDirectory)/up_tests_generic_tests_heter_queue_generic_tests.cpp$(ObjectSuffix) \
	$(IntermediateDirectory)/up_tests_generic_tests_queue_generic_tests.cpp$(ObjectSuffix) $(IntermediateDirectory)/up_tests_generic_tests_conc_heter_queue_generic_tests.cpp$(ObjectSuffix) 



Objects=$(Objects0) 

##
## Main Build Targets 
##
.PHONY: all clean PreBuild PrePreBuild PostBuild MakeIntermediateDirs
all: $(OutputFile)

$(OutputFile): $(IntermediateDirectory)/.d $(Objects) 
	@$(MakeDirCommand) $(@D)
	@echo "" > $(IntermediateDirectory)/.d
	@echo $(Objects0)  > $(ObjectsFileList)
	$(LinkerName) $(OutputSwitch)$(OutputFile) @$(ObjectsFileList) $(LibPath) $(Libs) $(LinkOptions)

MakeIntermediateDirs:
	@test -d ./Release || $(MakeDirCommand) ./Release


$(IntermediateDirectory)/.d:
	@test -d ./Release || $(MakeDirCommand) ./Release

PreBuild:


##
## Objects
##
$(IntermediateDirectory)/up_tests_nonblocking_heterogeneous_queue_basic_tests.cpp$(ObjectSuffix): ../tests/nonblocking_heterogeneous_queue_basic_tests.cpp $(IntermediateDirectory)/up_tests_nonblocking_heterogeneous_queue_basic_tests.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/giuseppe/Projects/density/density_tests/tests/nonblocking_heterogeneous_queue_basic_tests.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_tests_nonblocking_heterogeneous_queue_basic_tests.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_tests_nonblocking_heterogeneous_queue_basic_tests.cpp$(DependSuffix): ../tests/nonblocking_heterogeneous_queue_basic_tests.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_tests_nonblocking_heterogeneous_queue_basic_tests.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_tests_nonblocking_heterogeneous_queue_basic_tests.cpp$(DependSuffix) -MM ../tests/nonblocking_heterogeneous_queue_basic_tests.cpp

$(IntermediateDirectory)/up_tests_nonblocking_heterogeneous_queue_basic_tests.cpp$(PreprocessSuffix): ../tests/nonblocking_heterogeneous_queue_basic_tests.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_tests_nonblocking_heterogeneous_queue_basic_tests.cpp$(PreprocessSuffix) ../tests/nonblocking_heterogeneous_queue_basic_tests.cpp

$(IntermediateDirectory)/up_tests_concurrent_heterogeneous_queue_basic_tests.cpp$(ObjectSuffix): ../tests/concurrent_heterogeneous_queue_basic_tests.cpp $(IntermediateDirectory)/up_tests_concurrent_heterogeneous_queue_basic_tests.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/giuseppe/Projects/density/density_tests/tests/concurrent_heterogeneous_queue_basic_tests.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_tests_concurrent_heterogeneous_queue_basic_tests.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_tests_concurrent_heterogeneous_queue_basic_tests.cpp$(DependSuffix): ../tests/concurrent_heterogeneous_queue_basic_tests.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_tests_concurrent_heterogeneous_queue_basic_tests.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_tests_concurrent_heterogeneous_queue_basic_tests.cpp$(DependSuffix) -MM ../tests/concurrent_heterogeneous_queue_basic_tests.cpp

$(IntermediateDirectory)/up_tests_concurrent_heterogeneous_queue_basic_tests.cpp$(PreprocessSuffix): ../tests/concurrent_heterogeneous_queue_basic_tests.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_tests_concurrent_heterogeneous_queue_basic_tests.cpp$(PreprocessSuffix) ../tests/concurrent_heterogeneous_queue_basic_tests.cpp

$(IntermediateDirectory)/up_tests_load_unload_tests.cpp$(ObjectSuffix): ../tests/load_unload_tests.cpp $(IntermediateDirectory)/up_tests_load_unload_tests.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/giuseppe/Projects/density/density_tests/tests/load_unload_tests.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_tests_load_unload_tests.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_tests_load_unload_tests.cpp$(DependSuffix): ../tests/load_unload_tests.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_tests_load_unload_tests.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_tests_load_unload_tests.cpp$(DependSuffix) -MM ../tests/load_unload_tests.cpp

$(IntermediateDirectory)/up_tests_load_unload_tests.cpp$(PreprocessSuffix): ../tests/load_unload_tests.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_tests_load_unload_tests.cpp$(PreprocessSuffix) ../tests/load_unload_tests.cpp

$(IntermediateDirectory)/up_tests_heterogeneous_queue_basic_tests.cpp$(ObjectSuffix): ../tests/heterogeneous_queue_basic_tests.cpp $(IntermediateDirectory)/up_tests_heterogeneous_queue_basic_tests.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/giuseppe/Projects/density/density_tests/tests/heterogeneous_queue_basic_tests.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_tests_heterogeneous_queue_basic_tests.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_tests_heterogeneous_queue_basic_tests.cpp$(DependSuffix): ../tests/heterogeneous_queue_basic_tests.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_tests_heterogeneous_queue_basic_tests.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_tests_heterogeneous_queue_basic_tests.cpp$(DependSuffix) -MM ../tests/heterogeneous_queue_basic_tests.cpp

$(IntermediateDirectory)/up_tests_heterogeneous_queue_basic_tests.cpp$(PreprocessSuffix): ../tests/heterogeneous_queue_basic_tests.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_tests_heterogeneous_queue_basic_tests.cpp$(PreprocessSuffix) ../tests/heterogeneous_queue_basic_tests.cpp

$(IntermediateDirectory)/up_main.cpp$(ObjectSuffix): ../main.cpp $(IntermediateDirectory)/up_main.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/giuseppe/Projects/density/density_tests/main.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_main.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_main.cpp$(DependSuffix): ../main.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_main.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_main.cpp$(DependSuffix) -MM ../main.cpp

$(IntermediateDirectory)/up_main.cpp$(PreprocessSuffix): ../main.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_main.cpp$(PreprocessSuffix) ../main.cpp

$(IntermediateDirectory)/up_test_framework_progress.cpp$(ObjectSuffix): ../test_framework/progress.cpp $(IntermediateDirectory)/up_test_framework_progress.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/giuseppe/Projects/density/density_tests/test_framework/progress.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_test_framework_progress.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_test_framework_progress.cpp$(DependSuffix): ../test_framework/progress.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_test_framework_progress.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_test_framework_progress.cpp$(DependSuffix) -MM ../test_framework/progress.cpp

$(IntermediateDirectory)/up_test_framework_progress.cpp$(PreprocessSuffix): ../test_framework/progress.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_test_framework_progress.cpp$(PreprocessSuffix) ../test_framework/progress.cpp

$(IntermediateDirectory)/up_test_framework_histogram.cpp$(ObjectSuffix): ../test_framework/histogram.cpp $(IntermediateDirectory)/up_test_framework_histogram.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/giuseppe/Projects/density/density_tests/test_framework/histogram.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_test_framework_histogram.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_test_framework_histogram.cpp$(DependSuffix): ../test_framework/histogram.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_test_framework_histogram.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_test_framework_histogram.cpp$(DependSuffix) -MM ../test_framework/histogram.cpp

$(IntermediateDirectory)/up_test_framework_histogram.cpp$(PreprocessSuffix): ../test_framework/histogram.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_test_framework_histogram.cpp$(PreprocessSuffix) ../test_framework/histogram.cpp

$(IntermediateDirectory)/up_test_framework_line_updater_stream_adapter.cpp$(ObjectSuffix): ../test_framework/line_updater_stream_adapter.cpp $(IntermediateDirectory)/up_test_framework_line_updater_stream_adapter.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/giuseppe/Projects/density/density_tests/test_framework/line_updater_stream_adapter.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_test_framework_line_updater_stream_adapter.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_test_framework_line_updater_stream_adapter.cpp$(DependSuffix): ../test_framework/line_updater_stream_adapter.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_test_framework_line_updater_stream_adapter.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_test_framework_line_updater_stream_adapter.cpp$(DependSuffix) -MM ../test_framework/line_updater_stream_adapter.cpp

$(IntermediateDirectory)/up_test_framework_line_updater_stream_adapter.cpp$(PreprocessSuffix): ../test_framework/line_updater_stream_adapter.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_test_framework_line_updater_stream_adapter.cpp$(PreprocessSuffix) ../test_framework/line_updater_stream_adapter.cpp

$(IntermediateDirectory)/up_test_framework_dynamic_type.cpp$(ObjectSuffix): ../test_framework/dynamic_type.cpp $(IntermediateDirectory)/up_test_framework_dynamic_type.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/giuseppe/Projects/density/density_tests/test_framework/dynamic_type.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_test_framework_dynamic_type.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_test_framework_dynamic_type.cpp$(DependSuffix): ../test_framework/dynamic_type.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_test_framework_dynamic_type.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_test_framework_dynamic_type.cpp$(DependSuffix) -MM ../test_framework/dynamic_type.cpp

$(IntermediateDirectory)/up_test_framework_dynamic_type.cpp$(PreprocessSuffix): ../test_framework/dynamic_type.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_test_framework_dynamic_type.cpp$(PreprocessSuffix) ../test_framework/dynamic_type.cpp

$(IntermediateDirectory)/up_test_framework_test_objects.cpp$(ObjectSuffix): ../test_framework/test_objects.cpp $(IntermediateDirectory)/up_test_framework_test_objects.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/giuseppe/Projects/density/density_tests/test_framework/test_objects.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_test_framework_test_objects.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_test_framework_test_objects.cpp$(DependSuffix): ../test_framework/test_objects.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_test_framework_test_objects.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_test_framework_test_objects.cpp$(DependSuffix) -MM ../test_framework/test_objects.cpp

$(IntermediateDirectory)/up_test_framework_test_objects.cpp$(PreprocessSuffix): ../test_framework/test_objects.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_test_framework_test_objects.cpp$(PreprocessSuffix) ../test_framework/test_objects.cpp

$(IntermediateDirectory)/up_test_framework_threading_extensions.cpp$(ObjectSuffix): ../test_framework/threading_extensions.cpp $(IntermediateDirectory)/up_test_framework_threading_extensions.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/giuseppe/Projects/density/density_tests/test_framework/threading_extensions.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_test_framework_threading_extensions.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_test_framework_threading_extensions.cpp$(DependSuffix): ../test_framework/threading_extensions.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_test_framework_threading_extensions.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_test_framework_threading_extensions.cpp$(DependSuffix) -MM ../test_framework/threading_extensions.cpp

$(IntermediateDirectory)/up_test_framework_threading_extensions.cpp$(PreprocessSuffix): ../test_framework/threading_extensions.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_test_framework_threading_extensions.cpp$(PreprocessSuffix) ../test_framework/threading_extensions.cpp

$(IntermediateDirectory)/up_test_framework_density_test_common.cpp$(ObjectSuffix): ../test_framework/density_test_common.cpp $(IntermediateDirectory)/up_test_framework_density_test_common.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/giuseppe/Projects/density/density_tests/test_framework/density_test_common.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_test_framework_density_test_common.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_test_framework_density_test_common.cpp$(DependSuffix): ../test_framework/density_test_common.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_test_framework_density_test_common.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_test_framework_density_test_common.cpp$(DependSuffix) -MM ../test_framework/density_test_common.cpp

$(IntermediateDirectory)/up_test_framework_density_test_common.cpp$(PreprocessSuffix): ../test_framework/density_test_common.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_test_framework_density_test_common.cpp$(PreprocessSuffix) ../test_framework/density_test_common.cpp

$(IntermediateDirectory)/up_test_framework_shared_block_registry.cpp$(ObjectSuffix): ../test_framework/shared_block_registry.cpp $(IntermediateDirectory)/up_test_framework_shared_block_registry.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/giuseppe/Projects/density/density_tests/test_framework/shared_block_registry.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_test_framework_shared_block_registry.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_test_framework_shared_block_registry.cpp$(DependSuffix): ../test_framework/shared_block_registry.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_test_framework_shared_block_registry.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_test_framework_shared_block_registry.cpp$(DependSuffix) -MM ../test_framework/shared_block_registry.cpp

$(IntermediateDirectory)/up_test_framework_shared_block_registry.cpp$(PreprocessSuffix): ../test_framework/shared_block_registry.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_test_framework_shared_block_registry.cpp$(PreprocessSuffix) ../test_framework/shared_block_registry.cpp

$(IntermediateDirectory)/up_test_framework_exception_tests.cpp$(ObjectSuffix): ../test_framework/exception_tests.cpp $(IntermediateDirectory)/up_test_framework_exception_tests.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/giuseppe/Projects/density/density_tests/test_framework/exception_tests.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_test_framework_exception_tests.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_test_framework_exception_tests.cpp$(DependSuffix): ../test_framework/exception_tests.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_test_framework_exception_tests.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_test_framework_exception_tests.cpp$(DependSuffix) -MM ../test_framework/exception_tests.cpp

$(IntermediateDirectory)/up_test_framework_exception_tests.cpp$(PreprocessSuffix): ../test_framework/exception_tests.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_test_framework_exception_tests.cpp$(PreprocessSuffix) ../test_framework/exception_tests.cpp

$(IntermediateDirectory)/up_up_examples_concurrent_heterogeneous_queue_examples.cpp$(ObjectSuffix): ../../examples/concurrent_heterogeneous_queue_examples.cpp $(IntermediateDirectory)/up_up_examples_concurrent_heterogeneous_queue_examples.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/giuseppe/Projects/density/examples/concurrent_heterogeneous_queue_examples.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_up_examples_concurrent_heterogeneous_queue_examples.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_up_examples_concurrent_heterogeneous_queue_examples.cpp$(DependSuffix): ../../examples/concurrent_heterogeneous_queue_examples.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_up_examples_concurrent_heterogeneous_queue_examples.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_up_examples_concurrent_heterogeneous_queue_examples.cpp$(DependSuffix) -MM ../../examples/concurrent_heterogeneous_queue_examples.cpp

$(IntermediateDirectory)/up_up_examples_concurrent_heterogeneous_queue_examples.cpp$(PreprocessSuffix): ../../examples/concurrent_heterogeneous_queue_examples.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_up_examples_concurrent_heterogeneous_queue_examples.cpp$(PreprocessSuffix) ../../examples/concurrent_heterogeneous_queue_examples.cpp

$(IntermediateDirectory)/up_up_examples_heterogeneous_queue_examples.cpp$(ObjectSuffix): ../../examples/heterogeneous_queue_examples.cpp $(IntermediateDirectory)/up_up_examples_heterogeneous_queue_examples.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/giuseppe/Projects/density/examples/heterogeneous_queue_examples.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_up_examples_heterogeneous_queue_examples.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_up_examples_heterogeneous_queue_examples.cpp$(DependSuffix): ../../examples/heterogeneous_queue_examples.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_up_examples_heterogeneous_queue_examples.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_up_examples_heterogeneous_queue_examples.cpp$(DependSuffix) -MM ../../examples/heterogeneous_queue_examples.cpp

$(IntermediateDirectory)/up_up_examples_heterogeneous_queue_examples.cpp$(PreprocessSuffix): ../../examples/heterogeneous_queue_examples.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_up_examples_heterogeneous_queue_examples.cpp$(PreprocessSuffix) ../../examples/heterogeneous_queue_examples.cpp

$(IntermediateDirectory)/up_up_examples_nonblocking_heterogeneous_queue_examples.cpp$(ObjectSuffix): ../../examples/nonblocking_heterogeneous_queue_examples.cpp $(IntermediateDirectory)/up_up_examples_nonblocking_heterogeneous_queue_examples.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/giuseppe/Projects/density/examples/nonblocking_heterogeneous_queue_examples.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_up_examples_nonblocking_heterogeneous_queue_examples.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_up_examples_nonblocking_heterogeneous_queue_examples.cpp$(DependSuffix): ../../examples/nonblocking_heterogeneous_queue_examples.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_up_examples_nonblocking_heterogeneous_queue_examples.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_up_examples_nonblocking_heterogeneous_queue_examples.cpp$(DependSuffix) -MM ../../examples/nonblocking_heterogeneous_queue_examples.cpp

$(IntermediateDirectory)/up_up_examples_nonblocking_heterogeneous_queue_examples.cpp$(PreprocessSuffix): ../../examples/nonblocking_heterogeneous_queue_examples.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_up_examples_nonblocking_heterogeneous_queue_examples.cpp$(PreprocessSuffix) ../../examples/nonblocking_heterogeneous_queue_examples.cpp

$(IntermediateDirectory)/up_tests_generic_tests_nb_heter_queue_generic_tests_relaxed.cpp$(ObjectSuffix): ../tests/generic_tests/nb_heter_queue_generic_tests_relaxed.cpp $(IntermediateDirectory)/up_tests_generic_tests_nb_heter_queue_generic_tests_relaxed.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/giuseppe/Projects/density/density_tests/tests/generic_tests/nb_heter_queue_generic_tests_relaxed.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_tests_generic_tests_nb_heter_queue_generic_tests_relaxed.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_tests_generic_tests_nb_heter_queue_generic_tests_relaxed.cpp$(DependSuffix): ../tests/generic_tests/nb_heter_queue_generic_tests_relaxed.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_tests_generic_tests_nb_heter_queue_generic_tests_relaxed.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_tests_generic_tests_nb_heter_queue_generic_tests_relaxed.cpp$(DependSuffix) -MM ../tests/generic_tests/nb_heter_queue_generic_tests_relaxed.cpp

$(IntermediateDirectory)/up_tests_generic_tests_nb_heter_queue_generic_tests_relaxed.cpp$(PreprocessSuffix): ../tests/generic_tests/nb_heter_queue_generic_tests_relaxed.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_tests_generic_tests_nb_heter_queue_generic_tests_relaxed.cpp$(PreprocessSuffix) ../tests/generic_tests/nb_heter_queue_generic_tests_relaxed.cpp

$(IntermediateDirectory)/up_tests_generic_tests_nb_heter_queue_generic_tests_seqcst.cpp$(ObjectSuffix): ../tests/generic_tests/nb_heter_queue_generic_tests_seqcst.cpp $(IntermediateDirectory)/up_tests_generic_tests_nb_heter_queue_generic_tests_seqcst.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/giuseppe/Projects/density/density_tests/tests/generic_tests/nb_heter_queue_generic_tests_seqcst.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_tests_generic_tests_nb_heter_queue_generic_tests_seqcst.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_tests_generic_tests_nb_heter_queue_generic_tests_seqcst.cpp$(DependSuffix): ../tests/generic_tests/nb_heter_queue_generic_tests_seqcst.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_tests_generic_tests_nb_heter_queue_generic_tests_seqcst.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_tests_generic_tests_nb_heter_queue_generic_tests_seqcst.cpp$(DependSuffix) -MM ../tests/generic_tests/nb_heter_queue_generic_tests_seqcst.cpp

$(IntermediateDirectory)/up_tests_generic_tests_nb_heter_queue_generic_tests_seqcst.cpp$(PreprocessSuffix): ../tests/generic_tests/nb_heter_queue_generic_tests_seqcst.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_tests_generic_tests_nb_heter_queue_generic_tests_seqcst.cpp$(PreprocessSuffix) ../tests/generic_tests/nb_heter_queue_generic_tests_seqcst.cpp

$(IntermediateDirectory)/up_tests_generic_tests_heter_queue_generic_tests.cpp$(ObjectSuffix): ../tests/generic_tests/heter_queue_generic_tests.cpp $(IntermediateDirectory)/up_tests_generic_tests_heter_queue_generic_tests.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/giuseppe/Projects/density/density_tests/tests/generic_tests/heter_queue_generic_tests.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_tests_generic_tests_heter_queue_generic_tests.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_tests_generic_tests_heter_queue_generic_tests.cpp$(DependSuffix): ../tests/generic_tests/heter_queue_generic_tests.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_tests_generic_tests_heter_queue_generic_tests.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_tests_generic_tests_heter_queue_generic_tests.cpp$(DependSuffix) -MM ../tests/generic_tests/heter_queue_generic_tests.cpp

$(IntermediateDirectory)/up_tests_generic_tests_heter_queue_generic_tests.cpp$(PreprocessSuffix): ../tests/generic_tests/heter_queue_generic_tests.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_tests_generic_tests_heter_queue_generic_tests.cpp$(PreprocessSuffix) ../tests/generic_tests/heter_queue_generic_tests.cpp

$(IntermediateDirectory)/up_tests_generic_tests_queue_generic_tests.cpp$(ObjectSuffix): ../tests/generic_tests/queue_generic_tests.cpp $(IntermediateDirectory)/up_tests_generic_tests_queue_generic_tests.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/giuseppe/Projects/density/density_tests/tests/generic_tests/queue_generic_tests.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_tests_generic_tests_queue_generic_tests.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_tests_generic_tests_queue_generic_tests.cpp$(DependSuffix): ../tests/generic_tests/queue_generic_tests.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_tests_generic_tests_queue_generic_tests.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_tests_generic_tests_queue_generic_tests.cpp$(DependSuffix) -MM ../tests/generic_tests/queue_generic_tests.cpp

$(IntermediateDirectory)/up_tests_generic_tests_queue_generic_tests.cpp$(PreprocessSuffix): ../tests/generic_tests/queue_generic_tests.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_tests_generic_tests_queue_generic_tests.cpp$(PreprocessSuffix) ../tests/generic_tests/queue_generic_tests.cpp

$(IntermediateDirectory)/up_tests_generic_tests_conc_heter_queue_generic_tests.cpp$(ObjectSuffix): ../tests/generic_tests/conc_heter_queue_generic_tests.cpp $(IntermediateDirectory)/up_tests_generic_tests_conc_heter_queue_generic_tests.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/giuseppe/Projects/density/density_tests/tests/generic_tests/conc_heter_queue_generic_tests.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_tests_generic_tests_conc_heter_queue_generic_tests.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_tests_generic_tests_conc_heter_queue_generic_tests.cpp$(DependSuffix): ../tests/generic_tests/conc_heter_queue_generic_tests.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_tests_generic_tests_conc_heter_queue_generic_tests.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_tests_generic_tests_conc_heter_queue_generic_tests.cpp$(DependSuffix) -MM ../tests/generic_tests/conc_heter_queue_generic_tests.cpp

$(IntermediateDirectory)/up_tests_generic_tests_conc_heter_queue_generic_tests.cpp$(PreprocessSuffix): ../tests/generic_tests/conc_heter_queue_generic_tests.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_tests_generic_tests_conc_heter_queue_generic_tests.cpp$(PreprocessSuffix) ../tests/generic_tests/conc_heter_queue_generic_tests.cpp


-include $(IntermediateDirectory)/*$(DependSuffix)
##
## Clean
##
clean:
	$(RM) -r ./Release/


