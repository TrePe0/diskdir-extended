#include <windows.h>
#include "wcxhead.h"

/* mandatory functions */
typedef HANDLE (__stdcall *fOpenArchive)(tOpenArchiveData *ArchiveData);
typedef int (__stdcall *fReadHeader)(HANDLE hArcData, tHeaderData *HeaderData);
typedef int (__stdcall *fProcessFile)(HANDLE hArcData, int Operation, char *DestPath, char *DestName);
typedef int (__stdcall *fCloseArchive)(HANDLE hArcData);

/* optional function */
typedef int (__stdcall *fGetPackerCaps)(void);
typedef BOOL (__stdcall *fCanYouHandleThisFile)(char *FileName);
typedef int (__stdcall *fReadHeaderEx)(HANDLE hArcData, tHeaderDataEx *HeaderData);
