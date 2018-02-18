##
## Auto Generated makefile by CodeLite IDE
## any manual changes will be erased      
##
## Debug
ProjectName            :=CameraDeamon
ConfigurationName      :=Debug
WorkspacePath          := "/home/nvidia/CameraDeamon"
ProjectPath            := "/home/nvidia/CameraDeamon/CameraDeamon"
IntermediateDirectory  :=./Debug
OutDir                 := $(IntermediateDirectory)
CurrentFileName        :=
CurrentFilePath        :=
CurrentFileFullPath    :=
User                   :=
Date                   :=18/02/18
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
LinkOptions            :=  -pg `pkg-config opencv --cflags --libs` `/opt/pylon5/bin/pylon-config --libs-rpath` `pkg-config --libs libmongocxx` /home/nvidia/Downloads/CMake-hdf5-1.10.1/hdf5-1.10.1/hdf5/lib/libhdf5.so /usr/lib/aarch64-linux-gnu/libz.so /usr/lib/aarch64-linux-gnu/libdl.so /usr/lib/aarch64-linux-gnu/libm.so
IncludePath            :=  $(IncludeSwitch). $(IncludeSwitch)../lib $(IncludeSwitch)/usr/include/lib $(IncludeSwitch)/opt/pylon5/include $(IncludeSwitch)/usr/include/opencv2 $(IncludeSwitch)/home/agridata/CameraDeamon/CameraDeamon/library $(IncludeSwitch)/home/agridata/CameraDeamon/lib $(IncludeSwitch)/home/nvidia/Downloads/CMake-hdf5-1.10.1/hdf5-1.10.1/hdf5/include $(IncludeSwitch)/usr/include $(IncludeSwitch)/data/opencv_contrib/modules/xfeatures2d/include $(IncludeSwitch)/usr/local/cuda/include $(IncludeSwitch)/usr/local/cuda/lib64 $(IncludeSwitch)/data/AgriDataGPU/CudaSift 
IncludePCH             := 
RcIncludePath          := 
Libs                   := $(LibrarySwitch)pylonbase $(LibrarySwitch)pylonutility $(LibrarySwitch)GenApi_gcc_v3_0_Basler_pylon_v5_0 $(LibrarySwitch)GCBase_gcc_v3_0_Basler_pylon_v5_0 $(LibrarySwitch)boost_system $(LibrarySwitch)boost_filesystem $(LibrarySwitch)boost_python $(LibrarySwitch)zmq $(LibrarySwitch)pthread $(LibrarySwitch)profiler $(LibrarySwitch)hdf5 $(LibrarySwitch)hdf5_hl $(LibrarySwitch)redox $(LibrarySwitch)ev $(LibrarySwitch)hiredis 
ArLibs                 :=  "pylonbase" "pylonutility" "GenApi_gcc_v3_0_Basler_pylon_v5_0" "GCBase_gcc_v3_0_Basler_pylon_v5_0" "boost_system" "boost_filesystem" "boost_python" "zmq" "pthread" "profiler" "hdf5" "hdf5_hl" "redox" "ev" "hiredis" 
LibPath                := $(LibraryPathSwitch). $(LibraryPathSwitch)/opt/pylon5/lib64 $(LibraryPathSwitch)/usr/local/lib64 $(LibraryPathSwitch)/usr/local/cuda/lib64 

##
## Common variables
## AR, CXX, CC, AS, CXXFLAGS and CFLAGS can be overriden using an environment variables
##
AR       := /usr/bin/aarch64-linux-gnu-ar rcu
CXX      := /usr/bin/aarch64-linux-gnu-g++
CC       := /usr/bin/aarch64-linux-gnu-gcc
CXXFLAGS :=  -pg -O0 -w -std=c++11 -Wall -Wunknown-pragmas `/opt/pylon5/bin/pylon-config --cflags` `pkg-config --cflags libmongocxx` $(Preprocessors)
CFLAGS   :=  -pg -O0 -Wall $(Preprocessors)
ASFLAGS  := 
AS       := /usr/bin/aarch64-linux-gnu-as


##
## User defined environment variables
##
CodeLiteDir:=/usr/share/codelite
Objects0=$(IntermediateDirectory)/CameraDeamon_AgriDataCamera.cpp$(ObjectSuffix) $(IntermediateDirectory)/CameraDeamon_AGDUtils.cpp$(ObjectSuffix) $(IntermediateDirectory)/CameraDeamon_cudasift.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_easylogging++.cc$(ObjectSuffix) $(IntermediateDirectory)/lib_geomFuncs.cpp$(ObjectSuffix) 



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
	@test -d ./Debug || $(MakeDirCommand) ./Debug


$(IntermediateDirectory)/.d:
	@test -d ./Debug || $(MakeDirCommand) ./Debug

PreBuild:


##
## Objects
##
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

$(IntermediateDirectory)/CameraDeamon_cudasift.cpp$(ObjectSuffix): ../cudasift.cpp $(IntermediateDirectory)/CameraDeamon_cudasift.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/nvidia/CameraDeamon/cudasift.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/CameraDeamon_cudasift.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/CameraDeamon_cudasift.cpp$(DependSuffix): ../cudasift.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/CameraDeamon_cudasift.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/CameraDeamon_cudasift.cpp$(DependSuffix) -MM "../cudasift.cpp"

$(IntermediateDirectory)/CameraDeamon_cudasift.cpp$(PreprocessSuffix): ../cudasift.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/CameraDeamon_cudasift.cpp$(PreprocessSuffix) "../cudasift.cpp"

$(IntermediateDirectory)/lib_easylogging++.cc$(ObjectSuffix): ../lib/easylogging++.cc $(IntermediateDirectory)/lib_easylogging++.cc$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/nvidia/CameraDeamon/lib/easylogging++.cc" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_easylogging++.cc$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_easylogging++.cc$(DependSuffix): ../lib/easylogging++.cc
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_easylogging++.cc$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_easylogging++.cc$(DependSuffix) -MM "../lib/easylogging++.cc"

$(IntermediateDirectory)/lib_easylogging++.cc$(PreprocessSuffix): ../lib/easylogging++.cc
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_easylogging++.cc$(PreprocessSuffix) "../lib/easylogging++.cc"

$(IntermediateDirectory)/lib_geomFuncs.cpp$(ObjectSuffix): ../lib/geomFuncs.cpp $(IntermediateDirectory)/lib_geomFuncs.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/nvidia/CameraDeamon/lib/geomFuncs.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_geomFuncs.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_geomFuncs.cpp$(DependSuffix): ../lib/geomFuncs.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_geomFuncs.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_geomFuncs.cpp$(DependSuffix) -MM "../lib/geomFuncs.cpp"

$(IntermediateDirectory)/lib_geomFuncs.cpp$(PreprocessSuffix): ../lib/geomFuncs.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_geomFuncs.cpp$(PreprocessSuffix) "../lib/geomFuncs.cpp"


-include $(IntermediateDirectory)/*$(DependSuffix)
##
## Clean
##
clean:
	$(RM) -r ./Debug/


