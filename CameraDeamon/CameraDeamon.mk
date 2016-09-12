##
## Auto Generated makefile by CodeLite IDE
## any manual changes will be erased      
##
## Debug
ProjectName            :=CameraDeamon
ConfigurationName      :=Debug
WorkspacePath          :=/home/agridata/CameraDeamon
ProjectPath            :=/home/agridata/CameraDeamon/CameraDeamon
IntermediateDirectory  :=./Debug
OutDir                 := $(IntermediateDirectory)
CurrentFileName        :=
CurrentFilePath        :=
CurrentFileFullPath    :=
User                   :=agridata
Date                   :=12/09/16
CodeLitePath           :=/home/agridata/.codelite
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
LinkOptions            :=   `pkg-config opencv --cflags --libs` `/opt/pylon5/bin/pylon-config --libs-rpath`
IncludePath            :=  $(IncludeSwitch). $(IncludeSwitch)../lib $(IncludeSwitch)/usr/include/lib $(IncludeSwitch)/opt/pylon5/include/ $(IncludeSwitch)/usr/include/opencv2/ $(IncludeSwitch)/home/agridata/spdlog/include/ $(IncludeSwitch)/home/agridata/jeayeson/include/jeayeson/ $(IncludeSwitch)/home/agridata/CameraDeamon/CameraDeamon/library/ 
IncludePCH             := 
RcIncludePath          := 
Libs                   := $(LibrarySwitch)pylonbase $(LibrarySwitch)pylonutility $(LibrarySwitch)GenApi_gcc_v3_0_Basler_pylon_v5_0 $(LibrarySwitch)GCBase_gcc_v3_0_Basler_pylon_v5_0 $(LibrarySwitch)boost_python $(LibrarySwitch)zmq $(LibrarySwitch)pthread 
ArLibs                 :=  "pylonbase" "pylonutility" "GenApi_gcc_v3_0_Basler_pylon_v5_0" "GCBase_gcc_v3_0_Basler_pylon_v5_0" "boost_python" "zmq" "pthread" 
LibPath                := $(LibraryPathSwitch). $(LibraryPathSwitch)/opt/pylon5/lib64 $(LibraryPathSwitch)/usr/lib/x86_64-linux-gnu/ 

##
## Common variables
## AR, CXX, CC, AS, CXXFLAGS and CFLAGS can be overriden using an environment variables
##
AR       := ar rcus
CXX      := g++
CC       := gcc
CXXFLAGS :=  -g -O0 -Wall -std=c++11 -Wunknown-pragmas `/opt/pylon5/bin/pylon-config --cflags` $(Preprocessors)
CFLAGS   :=  -g -O0 -Wall $(Preprocessors)
ASFLAGS  := 
AS       := as


##
## User defined environment variables
##
CodeLiteDir:=/usr/share/codelite
Objects0=$(IntermediateDirectory)/main.cpp$(ObjectSuffix) 



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
$(IntermediateDirectory)/main.cpp$(ObjectSuffix): ../main.cpp $(IntermediateDirectory)/main.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/agridata/CameraDeamon/main.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/main.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/main.cpp$(DependSuffix): ../main.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/main.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/main.cpp$(DependSuffix) -MM ../main.cpp

$(IntermediateDirectory)/main.cpp$(PreprocessSuffix): ../main.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/main.cpp$(PreprocessSuffix)../main.cpp


-include $(IntermediateDirectory)/*$(DependSuffix)
##
## Clean
##
clean:
	$(RM) -r ./Debug/


