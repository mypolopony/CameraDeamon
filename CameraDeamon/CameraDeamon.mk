##
## Auto Generated makefile by CodeLite IDE
## any manual changes will be erased      
##
## Debug
ProjectName            :=CameraDeamon
ConfigurationName      :=Debug
WorkspacePath          := "/home/agridata/CameraDeamon"
ProjectPath            := "/home/agridata/CameraDeamon/CameraDeamon"
IntermediateDirectory  :=./Debug
OutDir                 := $(IntermediateDirectory)
CurrentFileName        :=
CurrentFilePath        :=
CurrentFileFullPath    :=
User                   :=agridata
Date                   :=14/03/17
CodeLitePath           :="/home/agridata/.codelite"
LinkerName             :=g++
SharedObjectLinkerName :=g++ -shared -fPIC
ObjectSuffix           :=.o
DependSuffix           :=.o.d
PreprocessSuffix       :=.o.i
DebugSwitch            :=-gstab
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
LinkOptions            :=  -g  `pkg-config opencv --cflags --libs` `/opt/pylon5/bin/pylon-config --libs-rpath` `pkg-config --libs libmongocxx`
IncludePath            :=  $(IncludeSwitch). $(IncludeSwitch)../lib $(IncludeSwitch)/usr/include/lib $(IncludeSwitch)/opt/pylon5/include/ $(IncludeSwitch)/usr/include/opencv2/ $(IncludeSwitch)/home/agridata/CameraDeamon/CameraDeamon/library/ 
IncludePCH             := 
RcIncludePath          := 
Libs                   := $(LibrarySwitch)pylonbase $(LibrarySwitch)pylonutility $(LibrarySwitch)GenApi_gcc_v3_0_Basler_pylon_v5_0 $(LibrarySwitch)GCBase_gcc_v3_0_Basler_pylon_v5_0 $(LibrarySwitch)boost_python $(LibrarySwitch)zmq $(LibrarySwitch)pthread
ArLibs                 :=  "pylonbase" "pylonutility" "GenApi_gcc_v3_0_Basler_pylon_v5_0" "GCBase_gcc_v3_0_Basler_pylon_v5_0" "boost_python" "zmq" "mongocxx" "bsoncxx" "boost_log" "boost_log_setup" "boost_system" "boost_thread" "boost_filesystem" "pthread" 
LibPath                := $(LibraryPathSwitch). $(LibraryPathSwitch)/opt/pylon5/lib64 

##
## Common variables
## AR, CXX, CC, AS, CXXFLAGS and CFLAGS can be overriden using an environment variables
##
AR       := ar rcus
CXX      := g++
CC       := gcc
CXXFLAGS :=  -g -O0 -w -std=c++11 -Wall -Wunknown-pragmas `/opt/pylon5/bin/pylon-config --cflags` `pkg-config --cflags libmongocxx` $(Preprocessors)
CFLAGS   :=  -g -O0 -Wall $(Preprocessors)
ASFLAGS  := 
AS       := as


##
## User defined environment variables
##
CodeLiteDir:=/usr/share/codelite
Objects0=$(IntermediateDirectory)/CameraDeamon_main.cpp$(ObjectSuffix) $(IntermediateDirectory)/CameraDeamon_AgriDataCamera.cpp$(ObjectSuffix) $(IntermediateDirectory)/CameraDeamon_AGDUtils.cpp$(ObjectSuffix) 



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
$(IntermediateDirectory)/CameraDeamon_main.cpp$(ObjectSuffix): ../main.cpp $(IntermediateDirectory)/CameraDeamon_main.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/agridata/CameraDeamon/main.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/CameraDeamon_main.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/CameraDeamon_main.cpp$(DependSuffix): ../main.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/CameraDeamon_main.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/CameraDeamon_main.cpp$(DependSuffix) -MM "../main.cpp"

$(IntermediateDirectory)/CameraDeamon_main.cpp$(PreprocessSuffix): ../main.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/CameraDeamon_main.cpp$(PreprocessSuffix) "../main.cpp"

$(IntermediateDirectory)/CameraDeamon_AgriDataCamera.cpp$(ObjectSuffix): ../AgriDataCamera.cpp $(IntermediateDirectory)/CameraDeamon_AgriDataCamera.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/agridata/CameraDeamon/AgriDataCamera.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/CameraDeamon_AgriDataCamera.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/CameraDeamon_AgriDataCamera.cpp$(DependSuffix): ../AgriDataCamera.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/CameraDeamon_AgriDataCamera.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/CameraDeamon_AgriDataCamera.cpp$(DependSuffix) -MM "../AgriDataCamera.cpp"

$(IntermediateDirectory)/CameraDeamon_AgriDataCamera.cpp$(PreprocessSuffix): ../AgriDataCamera.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/CameraDeamon_AgriDataCamera.cpp$(PreprocessSuffix) "../AgriDataCamera.cpp"

$(IntermediateDirectory)/CameraDeamon_AGDUtils.cpp$(ObjectSuffix): ../AGDUtils.cpp $(IntermediateDirectory)/CameraDeamon_AGDUtils.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/agridata/CameraDeamon/AGDUtils.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/CameraDeamon_AGDUtils.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/CameraDeamon_AGDUtils.cpp$(DependSuffix): ../AGDUtils.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/CameraDeamon_AGDUtils.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/CameraDeamon_AGDUtils.cpp$(DependSuffix) -MM "../AGDUtils.cpp"

$(IntermediateDirectory)/CameraDeamon_AGDUtils.cpp$(PreprocessSuffix): ../AGDUtils.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/CameraDeamon_AGDUtils.cpp$(PreprocessSuffix) "../AGDUtils.cpp"


-include $(IntermediateDirectory)/*$(DependSuffix)
##
## Clean
##
clean:
	$(RM) -r ./Debug/


