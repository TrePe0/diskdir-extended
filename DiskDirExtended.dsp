# Microsoft Developer Studio Project File - Name="DiskDirExtended" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=DiskDirExtended - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "DiskDirExtended.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "DiskDirExtended.mak" CFG="DiskDirExtended - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "DiskDirExtended - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "DiskDirExtended - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "DiskDirExtended - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "DiskDirExtended_EXPORTS" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "DiskDirExtended_EXPORTS" /D "HAVE_STRING_H" /D "HAVE_FCNTL_H" /D "STDC_HEADERS" /YX /FD /I./libs/zlib /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x405 /d "NDEBUG"
# ADD RSC /l 0x405 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib libs/cab/lib/fdi.lib /nologo /dll /machine:I386 /out:"Release\DiskDirExtended.wcx"
# SUBTRACT LINK32 /pdb:none
# Begin Special Build Tool
SOURCE="$(InputPath)"
PostBuild_Desc=Compress wcx
PostBuild_Cmds=upx\upx.exe -9 Release\DiskDirExtended.wcx
# End Special Build Tool

!ELSEIF  "$(CFG)" == "DiskDirExtended - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "DiskDirExtended_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "DiskDirExtended_EXPORTS" /YX /FD /GZ /I./libs/zlib /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x405 /d "_DEBUG"
# ADD RSC /l 0x405 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib libs/cab/lib/fdi.lib /nologo /dll /debug /machine:I386 /out:"Debug\DiskDirExtended.wcx" /pdbtype:sept
# SUBTRACT LINK32 /pdb:none

!ENDIF 

# Begin Target

# Name "DiskDirExtended - Win32 Release"
# Name "DiskDirExtended - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\DiskDirExtended.cpp
# End Source File
# Begin Source File

SOURCE=.\SortedList.cpp
# End Source File
# Begin Source File

SOURCE=.\wcx.def
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\defs.h
# End Source File
# Begin Source File

SOURCE=.\SortedList.h
# End Source File
# Begin Source File

SOURCE=.\wcxhead.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\DiskDirExtended.rc
# End Source File
# Begin Source File

SOURCE=.\resource.h
# End Source File
# Begin Source File

SOURCE=.\resrc1.h
# End Source File
# End Group
# Begin Group "ZIP"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\libs\zlib\adler32.c
# End Source File
# Begin Source File

SOURCE=.\libs\zlib\compress.c
# End Source File
# Begin Source File

SOURCE=.\libs\zlib\crc32.c
# End Source File
# Begin Source File

SOURCE=.\libs\zlib\crc32.h
# End Source File
# Begin Source File

SOURCE=.\libs\zlib\contrib\minizip\crypt.h
# End Source File
# Begin Source File

SOURCE=.\libs\zlib\deflate.c
# End Source File
# Begin Source File

SOURCE=.\libs\zlib\deflate.h
# End Source File
# Begin Source File

SOURCE=.\libs\zlib\gzio.c
# End Source File
# Begin Source File

SOURCE=.\libs\zlib\infback.c
# End Source File
# Begin Source File

SOURCE=.\libs\zlib\inffast.c
# End Source File
# Begin Source File

SOURCE=.\libs\zlib\inffast.h
# End Source File
# Begin Source File

SOURCE=.\libs\zlib\inffixed.h
# End Source File
# Begin Source File

SOURCE=.\libs\zlib\inflate.c
# End Source File
# Begin Source File

SOURCE=.\libs\zlib\inflate.h
# End Source File
# Begin Source File

SOURCE=.\libs\zlib\inftrees.c
# End Source File
# Begin Source File

SOURCE=.\libs\zlib\inftrees.h
# End Source File
# Begin Source File

SOURCE=.\libs\zlib\contrib\minizip\ioapi.c
# End Source File
# Begin Source File

SOURCE=.\libs\zlib\contrib\minizip\ioapi.h
# End Source File
# Begin Source File

SOURCE=.\libs\zlib\trees.c
# End Source File
# Begin Source File

SOURCE=.\libs\zlib\trees.h
# End Source File
# Begin Source File

SOURCE=.\libs\zlib\uncompr.c
# End Source File
# Begin Source File

SOURCE=.\libs\zlib\contrib\minizip\unzip.c
# End Source File
# Begin Source File

SOURCE=.\libs\zlib\contrib\minizip\unzip.h
# End Source File
# Begin Source File

SOURCE=.\libs\zlib\zconf.h
# End Source File
# Begin Source File

SOURCE=.\libs\zlib\zlib.h
# End Source File
# Begin Source File

SOURCE=.\libs\zlib\zutil.c
# End Source File
# Begin Source File

SOURCE=.\libs\zlib\zutil.h
# End Source File
# End Group
# Begin Group "RAR"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\libs\unrarlib\unrarlib.c
# End Source File
# Begin Source File

SOURCE=.\libs\unrarlib\unrarlib.h
# End Source File
# End Group
# Begin Group "CAB"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\libs\cab\Include\Fdi.h
# End Source File
# End Group
# Begin Group "TAR"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\libs\tar\src\tar.h
# End Source File
# End Group
# Begin Group "BZ2"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\libs\bzip2\blocksort.c

!IF  "$(CFG)" == "DiskDirExtended - Win32 Release"

# PROP Intermediate_Dir "Release\BZ2"

!ELSEIF  "$(CFG)" == "DiskDirExtended - Win32 Debug"

# PROP Intermediate_Dir "Debug\BZ2"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\libs\bzip2\bzlib.c

!IF  "$(CFG)" == "DiskDirExtended - Win32 Release"

# PROP Intermediate_Dir "Release\BZ2"

!ELSEIF  "$(CFG)" == "DiskDirExtended - Win32 Debug"

# PROP Intermediate_Dir "Debug\BZ2"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\libs\bzip2\bzlib.h

!IF  "$(CFG)" == "DiskDirExtended - Win32 Release"

# PROP Intermediate_Dir "Release\BZ2"

!ELSEIF  "$(CFG)" == "DiskDirExtended - Win32 Debug"

# PROP Intermediate_Dir "Debug\BZ2"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\libs\bzip2\bzlib_private.h

!IF  "$(CFG)" == "DiskDirExtended - Win32 Release"

# PROP Intermediate_Dir "Release\BZ2"

!ELSEIF  "$(CFG)" == "DiskDirExtended - Win32 Debug"

# PROP Intermediate_Dir "Debug\BZ2"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\libs\bzip2\compress.c

!IF  "$(CFG)" == "DiskDirExtended - Win32 Release"

# PROP Intermediate_Dir "Release\BZ2"

!ELSEIF  "$(CFG)" == "DiskDirExtended - Win32 Debug"

# PROP Intermediate_Dir "Debug\BZ2"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\libs\bzip2\crctable.c

!IF  "$(CFG)" == "DiskDirExtended - Win32 Release"

# PROP Intermediate_Dir "Release\BZ2"

!ELSEIF  "$(CFG)" == "DiskDirExtended - Win32 Debug"

# PROP Intermediate_Dir "Debug\BZ2"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\libs\bzip2\decompress.c

!IF  "$(CFG)" == "DiskDirExtended - Win32 Release"

# PROP Intermediate_Dir "Release\BZ2"

!ELSEIF  "$(CFG)" == "DiskDirExtended - Win32 Debug"

# PROP Intermediate_Dir "Debug\BZ2"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\libs\bzip2\huffman.c

!IF  "$(CFG)" == "DiskDirExtended - Win32 Release"

# PROP Intermediate_Dir "Release\BZ2"

!ELSEIF  "$(CFG)" == "DiskDirExtended - Win32 Debug"

# PROP Intermediate_Dir "Debug\BZ2"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\libs\bzip2\mk251.c

!IF  "$(CFG)" == "DiskDirExtended - Win32 Release"

# PROP Intermediate_Dir "Release\BZ2"

!ELSEIF  "$(CFG)" == "DiskDirExtended - Win32 Debug"

# PROP Intermediate_Dir "Debug\BZ2"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\libs\bzip2\randtable.c

!IF  "$(CFG)" == "DiskDirExtended - Win32 Release"

# PROP Intermediate_Dir "Release\BZ2"

!ELSEIF  "$(CFG)" == "DiskDirExtended - Win32 Debug"

# PROP Intermediate_Dir "Debug\BZ2"

!ENDIF 

# End Source File
# End Group
# End Target
# End Project
