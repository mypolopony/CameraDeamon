##
## Auto Generated makefile by CodeLite IDE
## any manual changes will be erased      
##
## Release
ProjectName            :=CameraDeamon
ConfigurationName      :=Release
WorkspacePath          := "/home/nvidia/CameraDeamon"
ProjectPath            := "/home/nvidia/CameraDeamon/CameraDeamon"
IntermediateDirectory  :=./Release
OutDir                 := $(IntermediateDirectory)
CurrentFileName        :=
CurrentFilePath        :=
CurrentFileFullPath    :=
User                   :=
Date                   :=27/02/18
CodeLitePath           :="/home/nvidia/.codelite"
LinkerName             :=/usr/bin/aarch64-linux-gnu-g++
SharedObjectLinkerName :=/usr/bin/aarch64-linux-gnu-g++ -shared -fPIC
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
Preprocessors          :=
ObjectSwitch           :=-o 
ArchiveOutputSwitch    := 
PreprocessOnlySwitch   :=-E
ObjectsFileList        :="CameraDeamon.txt"
PCHCompileFlags        :=
MakeDirCommand         :=mkdir -p
LinkOptions            :=  -pg -ggdb `pkg-config opencv --cflags --libs` `/opt/pylon5/bin/pylon-config --libs-rpath` `pkg-config --libs libmongocxx` /usr/lib/aarch64-linux-gnu/libz.so /usr/lib/aarch64-linux-gnu/libdl.so /usr/lib/aarch64-linux-gnu/libm.so
IncludePath            :=  $(IncludeSwitch). $(IncludeSwitch)lib $(IncludeSwitch)/usr/include/lib $(IncludeSwitch)/opt/pylon5/include $(IncludeSwitch)/usr/local/include/bsoncxx/v_noabi $(IncludeSwitch)/usr/local/include/mongocxx/v_noabi $(IncludeSwitch)/home/nvidia/CameraDeamon/lib $(IncludeSwitch)/usr/include/opencv2 $(IncludeSwitch)/usr/include $(IncludeSwitch)/data/opencv_contrib/modules/xfeatures2d/include $(IncludeSwitch)/data/CMake-hdf5-1.10.1/HDF_Group/HDF5/1.10.1/include 
IncludePCH             := 
RcIncludePath          := 
Libs                   := $(LibrarySwitch)pylonbase $(LibrarySwitch)pylonutility $(LibrarySwitch)GenApi_gcc_v3_0_Basler_pylon_v5_0 $(LibrarySwitch)GCBase_gcc_v3_0_Basler_pylon_v5_0 $(LibrarySwitch)boost_system $(LibrarySwitch)boost_filesystem $(LibrarySwitch)boost_python $(LibrarySwitch)zmq $(LibrarySwitch)pthread $(LibrarySwitch)profiler $(LibrarySwitch)hdf5 $(LibrarySwitch)hdf5_hl $(LibrarySwitch)redox $(LibrarySwitch)ev $(LibrarySwitch)hiredis 
ArLibs                 :=  "pylonbase" "pylonutility" "GenApi_gcc_v3_0_Basler_pylon_v5_0" "GCBase_gcc_v3_0_Basler_pylon_v5_0" "boost_system" "boost_filesystem" "boost_python" "zmq" "pthread" "profiler" "hdf5" "hdf5_hl" "redox" "ev" "hiredis" 
LibPath                := $(LibraryPathSwitch). $(LibraryPathSwitch)/opt/pylon5/lib64 $(LibraryPathSwitch)/usr/local/lib64 $(LibraryPathSwitch)/data/CMake-hdf5-1.10.1/HDF_Group/HDF5/1.10.1/lib 

##
## Common variables
## AR, CXX, CC, AS, CXXFLAGS and CFLAGS can be overriden using an environment variables
##
AR       := /usr/bin/aarch64-linux-gnu-ar rcu
CXX      := /usr/bin/aarch64-linux-gnu-g++
CC       := /usr/bin/aarch64-linux-gnu-gcc
CXXFLAGS :=  -pg -O0 -w -std=c++11 -Wall -Wunknown-pragmas -ggdb $(Preprocessors)
CFLAGS   :=  -pg -O0 -O2 -Wall $(Preprocessors)
ASFLAGS  := 
AS       := /usr/bin/aarch64-linux-gnu-as


##
## User defined environment variables
##
CodeLiteDir:=/usr/share/codelite
Objects0=$(IntermediateDirectory)/CameraDeamon_main.cpp$(ObjectSuffix) $(IntermediateDirectory)/CameraDeamon_AgriDataCamera.cpp$(ObjectSuffix) $(IntermediateDirectory)/CameraDeamon_AGDUtils.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_easylogging++.cc$(ObjectSuffix) 



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
$(IntermediateDirectory)/CameraDeamon_main.cpp$(ObjectSuffix): ../main.cpp $(IntermediateDirectory)/CameraDeamon_main.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/nvidia/CameraDeamon/main.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/CameraDeamon_main.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/CameraDeamon_main.cpp$(DependSuffix): ../main.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/CameraDeamon_main.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/CameraDeamon_main.cpp$(DependSuffix) -MM "../main.cpp"

$(IntermediateDirectory)/CameraDeamon_main.cpp$(PreprocessSuffix): ../main.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/CameraDeamon_main.cpp$(PreprocessSuffix) "../main.cpp"

$(IntermediateDirectory)/CameraDeamon_AgriDataCamera.cpp$(ObjectSuffix): ../AgriDataCamera.cpp $(IntermediateDirectory)/CameraDeamon_AgriDataCamera.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/nvidia/CameraDeamon/AgriDataCamera.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/CameraDeamon_AgriDataCamera.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/CameraDeamon_AgriDataCamera.cpp$(DependSuffix): ../AgriDataCamera.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/CameraDeamon_AgriDataCamera.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/CameraDeamon_AgriDataCamera.cpp$(DependSuffix) -MM "../AgriDataCamera.cpp"

$(IntermediateDirectory)/CameraDeamon_AgriDataCamera.cpp$(PreprocessSuffix): ../AgriDataCamera.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/CameraDeamon_AgriDataCamera.cpp$(PreprocessSuffix) "../AgriDataCamera.cpp"

$(IntermediateDirectory)/CameraDeamon_AGDUtils.cpp$(ObjectSuffix): ../AGDUtils.cpp $(IntermediateDirectory)/CameraDeamon_AGDUtils.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/nvidia/CameraDeamon/AGDUtils.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/CameraDeamon_AGDUtils.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/CameraDeamon_AGDUtils.cpp$(DependSuffix): ../AGDUtils.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/CameraDeamon_AGDUtils.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/CameraDeamon_AGDUtils.cpp$(DependSuffix) -MM "../AGDUtils.cpp"

$(IntermediateDirectory)/CameraDeamon_AGDUtils.cpp$(PreprocessSuffix): ../AGDUtils.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/CameraDeamon_AGDUtils.cpp$(PreprocessSuffix) "../AGDUtils.cpp"

$(IntermediateDirectory)/lib_easylogging++.cc$(ObjectSuffix): ../lib/easylogging++.cc $(IntermediateDirectory)/lib_easylogging++.cc$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/nvidia/CameraDeamon/lib/easylogging++.cc" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_easylogging++.cc$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_easylogging++.cc$(DependSuffix): ../lib/easylogging++.cc
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_easylogging++.cc$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_easylogging++.cc$(DependSuffix) -MM "../lib/easylogging++.cc"

$(IntermediateDirectory)/lib_easylogging++.cc$(PreprocessSuffix): ../lib/easylogging++.cc
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_easylogging++.cc$(PreprocessSuffix) "../lib/easylogging++.cc"


-include $(IntermediateDirectory)/*$(DependSuffix)
##
## Clean
##
clean:
	$(RM) -r ./Release/


