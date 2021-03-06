// (c) 2009 Peter Trebaticky
// DiskDir Extended v1.68, plugin for Total Commander, www.ghisler.com
// Last change: 2013/09/18

#include "wcxhead.h"
#include "defs.h"
#include "DirTree.h"
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include "libs/zlib/contrib/minizip/unzip.h"
#include "libs/unrarlib/unrarlib.h"
#include "libs/tar/src/tar.h"
#include "libs/cab/include/fdi.h"
#include "libs/bzip2/bzlib.h"
#include "libs/iso/iso_wincmd.h"
#include "api_totalcmd.h"

#include "resource.h"
#include "resrc1.h"
#include <commctrl.h>
#include "settings.h"

using namespace std;

// for ReadHeader+ProcessFile as well as PackFiles
char curPath[MAX_FULL_PATH_LEN];
char curDestPath[MAX_FULL_PATH_LEN];

char curFileName[MAX_FULL_PATH_LEN];
char mainSrcPath[MAX_FULL_PATH_LEN]; // first line of .lst file
tProcessDataProc progressFunction;
tChangeVolProc changeVolFunction;
char todayStr[20]; // "%d.%d.%d\t%d:%d.%d", year, month, day, hour, minute, second
int basePathCnt = 0;
HINSTANCE dllInstance;
DirTree* sortedList = NULL; // global because of CAB archives
FILE* fout; // file being created
int ListZIP(const char* fileName);
int ListRAR(const char* fileName);
int ListACE(const char* fileName);
int ListARJ(const char* fileName);
int ListCAB(const char* fileName);
int ListISO(const char* fileName);
int ListTAR(const char* fileName, int fType);
int ListByWCX(const char* wcx_path, const char* fileName);
bool firstLineRead; // if the first line from lst file was read (= basePath)
bool notForTC; // used in ReadHeaderEx - do not remove "c:\" part of file name

HANDLE __stdcall OpenArchive (tOpenArchiveData *archiveData)
{
	HANDLE f;

	switch(archiveData->OpenMode)
	{
		case PK_OM_LIST:
		case PK_OM_EXTRACT:
			f = fopen(archiveData->ArcName, "rt");
			if(f == NULL)
			{
				archiveData->OpenResult = E_EOPEN;
				return 0;
			}
			curPath[0] = '\0';
			mainSrcPath[0] = '\0';
			firstLineRead = false;
			notForTC = false;
			settings.readConfig();
			return f;
		break;
		default:
			archiveData->OpenResult = E_NOT_SUPPORTED;
			return 0;
	}
}

int __stdcall ReadHeader (HANDLE handle, tHeaderData *headerData)
{
	char str[MAX_FULL_PATH_LEN];
	char *retval, *lom;

again:
	retval = fgets(str, MAX_FULL_PATH_LEN, (FILE*)handle);
	lom = strrchr(str, '\\');
	while(retval != NULL && ((str[0] == '\n' || str[0] == '\0') || (!firstLineRead && strchr(str, '\t') == NULL && lom != NULL && *(lom+1) <= '\n')))
	{
		if (!(str[0] == '\n' || str[0] == '\0')) {
			strncpy(mainSrcPath, str, MAX_FULL_PATH_LEN); mainSrcPath[MAX_FULL_PATH_LEN - 1] = '\0';
			if(mainSrcPath[strlen(mainSrcPath) - 1] == '\n')
				mainSrcPath[strlen(mainSrcPath) - 1] = '\0';
			if(mainSrcPath[0] != '\0' && mainSrcPath[strlen(mainSrcPath) - 1] != '\\')
				strcat(mainSrcPath, "\\");
			firstLineRead = true;
		}
		retval = fgets(str, MAX_FULL_PATH_LEN, (FILE*)handle);
		lom = strrchr(str, '\\');
	}

	if(retval != NULL)
	{
		firstLineRead = true;
		int i, cnt, year, month, day, hour, minute, second;

		i = (int)strchr(str, '\t') - (int)str;
		// fix for nonstandard lst files (no size and date)
		if (i < 0) {i = strlen(str); if (i > 0 && str[i - 1] == '\n') i--;}

		// if not directory or separator
		// or it contains full path
		if(i != 0 && str[i - 1] != '\\' && !(str[1] == ':' && str[2] == '\\')) 
		{
			// prepend current directory
			strcpy_s(headerData->FileName, MAX_FILE_NAME_LEN, curPath);
			strncat_s(headerData->FileName, MAX_FILE_NAME_LEN, str, i);
			headerData->FileName[min(strlen(curPath) + i, MAX_FILE_NAME_LEN - 1)] = '\0';
		}
		else
		{
			strncpy_s(headerData->FileName, MAX_FILE_NAME_LEN, str, i);
			headerData->FileName[min(i, MAX_FILE_NAME_LEN - 1)] = '\0';
		}
		headerData->UnpSize = 0;
		year = month = day = hour = minute = second = 0;
		cnt = sscanf(str + i, "%d\t%d.%d.%d\t%d:%d.%d", &(headerData->UnpSize),
			&year, &month, &day,
			&hour, &minute, &second);
		headerData->FileTime = (cnt < 4) ? -1 :
			((year - 1980) << 25) | (month << 21) | (day << 16) |
			(hour << 11) | (minute << 5) | (second/2);

		if (notForTC) if (settings.getListColumns() < cnt) settings.setListColumns(cnt);

		if(i == 0 || str[i - 1] == '\\')
		{
			strcpy(curPath, headerData->FileName);
			if(i == 0) goto again; // separator
			// this way the packed file size will be displayed
			if(headerData->UnpSize == 0) headerData->FileAttr = 16;
		}
		else headerData->FileAttr = 0;

		headerData->CmtBuf = NULL;
		headerData->CmtBufSize = 0;
		headerData->CmtSize = 0;
		headerData->CmtState = 0;
		headerData->FileCRC = 0;
		headerData->HostOS = 0;

		strcpy(curFileName, headerData->FileName);
		if (headerData->FileName[1] == ':' && 
			headerData->FileName[2] == '\\')
		{
			if (headerData->FileName[3] == '\0') goto again; // skip c:\, x:\, etc.
			// do not supply c:\ part (TC does not handle it properly)
			for (i = 3; headerData->FileName[i] != '\0'; i++) {
				headerData->FileName[i - 3] = headerData->FileName[i];
			}
			headerData->FileName[i - 3] = '\0';
		}
	}
	return retval == NULL ? E_END_ARCHIVE : 0;
}

int __stdcall ReadHeaderEx (HANDLE handle, tHeaderDataEx *headerData)
{
	char str[MAX_FULL_PATH_LEN];
	char *retval, *lom;
	unsigned __int64 fSize;

again:
	retval = fgets(str, MAX_FULL_PATH_LEN, (FILE*)handle);
	lom = strrchr(str, '\\');
	while(retval != NULL && ((str[0] == '\n' || str[0] == '\0') || (!firstLineRead && strchr(str, '\t') == NULL && lom != NULL && *(lom+1) <= '\n')))
	{
		if (!(str[0] == '\n' || str[0] == '\0')) {
			strncpy(mainSrcPath, str, MAX_FULL_PATH_LEN); mainSrcPath[MAX_FULL_PATH_LEN - 1] = '\0';
			if(mainSrcPath[strlen(mainSrcPath) - 1] == '\n')
				mainSrcPath[strlen(mainSrcPath) - 1] = '\0';
			if(mainSrcPath[0] != '\0' && mainSrcPath[strlen(mainSrcPath) - 1] != '\\')
				strcat(mainSrcPath, "\\");
			firstLineRead = true;
		}
		retval = fgets(str, MAX_FULL_PATH_LEN, (FILE*)handle);
		lom = strrchr(str, '\\');
	}

	if(retval != NULL)
	{
		firstLineRead = true;
		int i, cnt, year, month, day, hour, minute, second;

		i = (int)strchr(str, '\t') - (int)str;
		// fix for nonstandard lst files (no size and date)
		if (i < 0) {i = strlen(str); if (i > 0 && str[i - 1] == '\n') i--;}

		// if not directory or separator
		// or it contains full path
		if(i != 0 && str[i - 1] != '\\' && !(str[1] == ':' && str[2] == '\\')) 
		{
			// prepend current directory
			strcpy_s(headerData->FileName, MAX_FILE_NAME_LEN_EX, curPath);
			strncat_s(headerData->FileName, MAX_FILE_NAME_LEN_EX, str, i);
			headerData->FileName[min(strlen(curPath) + i, MAX_FILE_NAME_LEN_EX - 1)] = '\0';
		}
		else
		{
			strncpy_s(headerData->FileName, MAX_FILE_NAME_LEN_EX, str, i);
			headerData->FileName[min(i, MAX_FILE_NAME_LEN_EX - 1)] = '\0';
		}
		headerData->UnpSize = 0;
		headerData->UnpSizeHigh = 0;
		fSize = year = month = day = hour = minute = second = 0;
		cnt = sscanf(str + i, "%llu\t%d.%d.%d\t%d:%d.%d", &fSize,
			&year, &month, &day,
			&hour, &minute, &second);
		headerData->FileTime = ((cnt < 4 && !notForTC) || (notForTC && cnt < 2)) ? -1 :
			((year - 1980) << 25) | (month << 21) | (day << 16) |
			(hour << 11) | (minute << 5) | (second/2);

		if (notForTC) if (settings.getListColumns() < cnt + 1) settings.setListColumns(cnt + 1);

		if(i == 0 || str[i - 1] == '\\')
		{
			strcpy(curPath, headerData->FileName);
			if(i == 0) goto again; // separator
			// this way the packed file size will be displayed
			if(fSize == 0) headerData->FileAttr = 16;
		}
		else headerData->FileAttr = 0;

		headerData->UnpSize = (int) fSize;
		headerData->UnpSizeHigh = (int) _rotr64(fSize, 32);
		headerData->CmtBuf = NULL;
		headerData->CmtBufSize = 0;
		headerData->CmtSize = 0;
		headerData->CmtState = 0;
		headerData->FileCRC = 0;
		headerData->HostOS = 0;
		memset(headerData->Reserved, 0, 1024);

		strcpy(curFileName, headerData->FileName);
		if (headerData->FileName[1] == ':' && 
			headerData->FileName[2] == '\\')
		{
			if (headerData->FileName[3] == '\0') goto again; // skip c:\, x:\, etc.
			if (!notForTC) {
				// do not supply c:\ part (TC does not handle it properly)
				for (i = 3; headerData->FileName[i] != '\0'; i++) {
					headerData->FileName[i - 3] = headerData->FileName[i];
				}
				headerData->FileName[i - 3] = '\0';
			}
		}
	}
	return retval == NULL ? E_END_ARCHIVE : 0;
}

int __stdcall ProcessFile (HANDLE handle, int operation, char *destPath, char *destName)
{
	static char wholeNameSrc[MAX_FULL_PATH_LEN];
	static char wholeNameDest[MAX_FULL_PATH_LEN];
	static char buf[MAX_FULL_PATH_LEN];
	FILE *fin, *fout;
	int i;

	switch(operation)
	{
		case PK_SKIP:
		case PK_TEST:
			return 0;
		case PK_EXTRACT:
			// copy mainSrcPath + curPath to destPath + destName
			strcpy(wholeNameSrc, mainSrcPath);
			strcat(wholeNameSrc, curFileName);
			if(destPath != NULL) strcpy(wholeNameDest, destPath);
			else wholeNameDest[0] = '\0';
			strcat(wholeNameDest, destName);
			fin = fopen(wholeNameSrc, "rb");
			while(fin == NULL)
			{
				if(changeVolFunction(wholeNameSrc, PK_VOL_ASK) == 0) return 0;
				fin = fopen(wholeNameSrc, "rb");
			}

			fout = fopen(wholeNameDest, "wb");
			if(fout == NULL)
			{
				fclose(fin);
				return E_ECREATE;
			}
			i = fread(buf, 1, MAX_FULL_PATH_LEN, fin);
			while(i > 0)
			{
				fwrite(buf, i, 1, fout);
				if (progressFunction(wholeNameSrc, i) == 0) {
					fclose(fin);
					fclose(fout);
					unlink(wholeNameDest);
					return E_EABORTED;
				}
				i = fread(buf, 1, MAX_FULL_PATH_LEN, fin);
			}
			fclose(fin);
			fclose(fout);

		return 0;
		default:
			return E_NOT_SUPPORTED;
	}
	return E_END_ARCHIVE;
}

int __stdcall CloseArchive (HANDLE handle)
{
	fclose((FILE*) handle);
	return 0;
}

void __stdcall SetChangeVolProc (HANDLE hArcData, tChangeVolProc pChangeVolProc)
{
	changeVolFunction = pChangeVolProc;
}

void __stdcall SetProcessDataProc (HANDLE hArcData, tProcessDataProc pProcessDataProc)
{
	progressFunction = pProcessDataProc;
}

int __stdcall GetPackerCaps()
{
	return PK_CAPS_NEW | PK_CAPS_MODIFY | PK_CAPS_DELETE | PK_CAPS_MULTIPLE | PK_CAPS_OPTIONS;
}


/*
  Reads the contents of open file and fills sortedList
  overwrites mainSrcPath with the one from file
*/
void FillSortedListFromFile(DirTree **sortedList, FILE* fin) {
	if (*sortedList != NULL) delete *sortedList;
	*sortedList = new DirTree();

	curPath[0] = '\0';
	mainSrcPath[0] = '\0';
	firstLineRead = false;

	tHeaderDataEx t;
	curDestPath[0] = '\0';
	notForTC = true;
	settings.setListColumns(1); // listColumns will be determined from file
	while (ReadHeaderEx(fin, &t) == 0) {
		(*sortedList)->insert(curDestPath, t.FileName, (unsigned)t.UnpSize + _rotl64((unsigned)t.UnpSizeHigh, 32), t.FileTime, (t.FileAttr & 16) ? FILE_TYPE_DIRECTORY : FILE_TYPE_REGULAR);
	}
	notForTC = false;
}

int __stdcall DeleteFiles (char *PackedFile, char *deleteList)
{
	FILE *fout = fopen(PackedFile, "rt");
	if (fout == NULL) return E_EOPEN;
	DirTree *sortedList = NULL;
	FillSortedListFromFile(&sortedList, fout);
	fclose(fout);

	fout = fopen(PackedFile, "wt");
	if (fout == NULL) {
		delete sortedList;
		return E_EOPEN;
	}

	if (mainSrcPath[0] != '\0') fprintf(fout, "%s\n", mainSrcPath);

	int i = 0;
	char fName[MAX_FULL_PATH_LEN];

	while(deleteList[i] != '\0')
	{
		strcpy(fName, "");
		strcpy(fName, deleteList + i);
		int n = strlen(fName);
		if (n > 4) { // delete whole directory
			if (fName[n-3] == '*' && fName[n-2] == '.' && fName[n-1] == '*')
				fName[n-3] = '\0';
		}
		sortedList->remove(fName);
		i += strlen(deleteList + i) + 1;
	}

	curPath[0] = '\0';
	sortedList->writeOut(fout, curPath);
	delete sortedList;

	fclose(fout);

	return 0;
}

int DetermineFileTypeFromName(const char* fName) {
	int fNameLen = strlen(fName);
	if(fName[fNameLen - 1] == '\\') return FILE_TYPE_DIRECTORY;

	int i;
	char *fNameCI = (char*) malloc(fNameLen+1);
	for (i = 0; fName[i] != '\0'; i++) fNameCI[i] = tolower(fName[i]);
	fNameCI[i] = '\0';
	for (i = 0; fNameCI[i] != '\0'; i++) {
		if (fNameCI[i] == '.') {
			settings.fileTypeMapIt = settings.fileTypeMap.find(fNameCI+i);
			if (settings.fileTypeMapIt != settings.fileTypeMap.end()) {
				settings.which_wcx = settings.fileTypeMapIt->second.which_wcx;
				free(fNameCI);
				return settings.fileTypeMapIt->second.fileType;
			}
		}
	}
	free(fNameCI);
	return FILE_TYPE_REGULAR;
}

bool DetermineWcxHandleableFromName(const char* fName) {
	if (settings.wcxHandleableSet.empty()) return true;
	int fNameLen = strlen(fName);
	if(fName[fNameLen - 1] == '\\') return false;

	int i;
	char *fNameCI = (char*) malloc(fNameLen+1);
	for (i = 0; fName[i] != '\0'; i++) fNameCI[i] = tolower(fName[i]);
	fNameCI[i] = '\0';
	for (i = 0; fNameCI[i] != '\0'; i++) {
		if (fNameCI[i] == '.') {
			settings.wcxHandleableSetIt = settings.wcxHandleableSet.find(fNameCI+i);
			if (settings.wcxHandleableSetIt != settings.wcxHandleableSet.end()) {
				free(fNameCI);
				return true;
			}
		}
	}
	free(fNameCI);
	return false;
}

int DetermineFileType(const char *fName)
{
	int fType = DetermineFileTypeFromName(fName);
	// directory added by F7 does not exist
	if (fType == FILE_TYPE_REGULAR && settings.getTryCanYouHandleThisFile() && DetermineWcxHandleableFromName(fName)) {
		// try to determine file type using wcx plugins' CanYouHandleThisFile

		for (settings.which_wcx = settings.wcxmap.begin(); settings.which_wcx != settings.wcxmap.end(); ++settings.which_wcx) {
			if (settings.getCanYouHandleThisFile(settings.which_wcx)) {
				HMODULE hwcx = NULL;
				if(!(hwcx = LoadLibrary(settings.which_wcx->second.first.c_str()))) {
					return fType;
				}
				fCanYouHandleThisFile pCanYouHandleThisFile = NULL;
				pCanYouHandleThisFile = (fCanYouHandleThisFile) GetProcAddress(hwcx, "CanYouHandleThisFile");
				if (pCanYouHandleThisFile((char*)fName)) {
					fType = FILE_TYPE_BY_WCX;
					FreeLibrary(hwcx);
					break;
				}
				FreeLibrary(hwcx);
			}
		}
	}
	return fType;
}

LIST_OPTION_ENUM GetFileListType(const char *fName) {
	int i;
	char *fNameCI = (char*) malloc(strlen(fName)+1);
	for (i = 0; fName[i] != '\0'; i++) fNameCI[i] = tolower(fName[i]);
	fNameCI[i] = '\0';
	for (i = 0; fNameCI[i] != '\0'; i++) {
		if (fNameCI[i] == '.') {
			settings.fileTypeMapIt = settings.fileTypeMap.find(fNameCI+i);
			if (settings.fileTypeMapIt != settings.fileTypeMap.end()) {
				free(fNameCI);
				return settings.fileTypeMapIt->second.list_this;
			}
		}
	}
	free(fNameCI);
	return LIST_YES;
}

void SetFileListType(const char *fName, LIST_OPTION_ENUM list_this) {
	int i;
	for (i = 0; fName[i] != '\0'; i++) {
		if (fName[i] == '.') {
			settings.fileTypeMapIt = settings.fileTypeMap.find(fName+i);
			if (settings.fileTypeMapIt != settings.fileTypeMap.end()) {
				settings.fileTypeMapIt->second.list_this = list_this;
				return;
			}
		}
	}
}

BOOL CALLBACK DialogFunc2(HWND hdwnd, UINT Msg, WPARAM wParam, LPARAM lParam);

int __stdcall PackFiles(char *packedFile, char *subPath, char *srcPath, char *addList, int flags)
{
	int i, j;
	char str[MAX_FULL_PATH_LEN];
	char fName[MAX_FULL_PATH_LEN];
//	char fDestName[MAX_FULL_PATH_LEN];
	int fType;
	unsigned timeStamp;

	strcpy(mainSrcPath, srcPath);

	// are we updateing?
	fout = fopen(packedFile, "rt");
	if (fout != NULL) {
		FillSortedListFromFile(&sortedList, fout);
		fclose(fout);
/*
		if (strcmp(mainSrcPath, srcPath) != 0) {
			MessageBox(NULL, "You are adding files from different directory", "DiskDirExtended - Question", MB_ICONQUESTION | MB_OKCANCEL);
			return E_EABORTED;
		}
*/
	} else {
		if (sortedList != NULL) delete sortedList;
		sortedList = new DirTree();
	}

	int oldListColumns = settings.getListColumns();
	settings.readConfig();
	if (settings.getListColumns() != oldListColumns) {
		char res[8192];
		char msg[8192];
		LoadString(dllInstance, IDS_LIST_COLUMNS_QUERY, res, 8192);
		sprintf_s(msg, res, settings.getListColumns(), oldListColumns, oldListColumns);
		LoadString(dllInstance, IDS_LIST_COLUMNS_TITLE, res, 8192);
		switch (MessageBox(NULL, msg, res, MB_ICONQUESTION | MB_YESNOCANCEL | MB_TASKMODAL)) {
			case IDYES:
				settings.setListColumns(oldListColumns);
			break;
			case IDNO:
				// listColumns already are what user wants
			break;
			default:
				return E_EABORTED;
		}
	}

	curPath[0] = '\0';
	curDestPath[0] = '\0';
	if(subPath != NULL)
	{
		strcpy(curDestPath, subPath);
		if(curDestPath[strlen(curDestPath) - 1] != '\\') strcat(curDestPath, "\\");
	}

	i = 0;
	while(addList[i] != '\0')
	{
		strcpy(str, srcPath);
		if(str[0] != '\0' && str[strlen(str) - 1] != '\\') strcat(str, "\\");
		strcat(str, addList + i);

		WIN32_FILE_ATTRIBUTE_DATA buf;
		FILETIME t;
		SYSTEMTIME ts;

		GetFileAttributesEx(str, GetFileExInfoStandard, (LPVOID)(&buf));
		// sometimes we get directory without ending backslash
		if (buf.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) if (str[strlen(str) - 1] != '\\') strcat(str, "\\");
		
		fType = DetermineFileType(str);
		if(fType == FILE_TYPE_DIRECTORY)
		{
			FileTimeToLocalFileTime(&(buf.ftLastWriteTime), &t);
			str[strlen(str) - 1] = '\\';
			buf.nFileSizeLow = 0;
		}
		else FileTimeToLocalFileTime(&(buf.ftLastWriteTime), &t);
		FileTimeToSystemTime(&t, &ts);

		timeStamp = ((ts.wYear - 1980) << 25) | (ts.wMonth << 21) | (ts.wDay << 16) |
					(ts.wHour << 11) | (ts.wMinute << 5) | (ts.wSecond / 2);

		strcpy(fName, addList + i);
		if (progressFunction(fName, 0) == 0) {
			delete sortedList;
			sortedList = NULL;
			return E_EABORTED;
		}

		if (fType != FILE_TYPE_DIRECTORY) {
			// if we do not want to list any archive OR
			// if we do not want to list any file except directories
			if (!settings.getListArchives() || settings.getListOnlyDirectories()) fType = FILE_TYPE_REGULAR;
			else
			switch(GetFileListType(fName))
			{
				case LIST_NO:
					fType = FILE_TYPE_REGULAR;
					break;
				case LIST_ASK:
					switch(DialogBoxParam(dllInstance, MAKEINTRESOURCE(IDD_LIST_FILE), NULL, DialogFunc2, (LPARAM)fName))
					{
					case 1: // No
						fType = FILE_TYPE_REGULAR;
						break;
					case 2: // Yes for all
						SetFileListType(fName, LIST_YES);
						break;
					case 3: // No for all
						SetFileListType(fName, LIST_NO);
						fType = FILE_TYPE_REGULAR;
						break;
					}
					break;
				case LIST_YES:
					break;
			}
		} else {
			if (fName[strlen(fName) - 1] != '\\') strcat(fName,"\\");
		}
		switch(fType)
		{
			case FILE_TYPE_BY_WCX:
				strcat(fName, "\\");
				sortedList->insert(curDestPath, fName, buf.nFileSizeHigh * 0x100000000ULL + buf.nFileSizeLow, timeStamp, FILE_TYPE_DIRECTORY);
				strcpy(curPath, fName);
				basePathCnt = strlen(curPath);
				if ((j = ListByWCX(settings.which_wcx->second.first.c_str(), str)) < 0) {
					delete sortedList;
					sortedList = NULL;
					return E_EABORTED;
				} else if (j == 0 && !settings.getListEmptyFile()) {
					sortedList->doNotListAsDirLastInserted();
				}
			break;
			case FILE_TYPE_ZIP:
			case FILE_TYPE_JAR:
				strcat(fName, "\\");
				sortedList->insert(curDestPath, fName, buf.nFileSizeHigh * 0x100000000ULL + buf.nFileSizeLow, timeStamp, FILE_TYPE_DIRECTORY);
				strcpy(curPath, fName);
				basePathCnt = strlen(curPath);
				if ((j = ListZIP(str)) < 0) {
					delete sortedList;
					sortedList = NULL;
					return E_EABORTED;
				} else if (j == 0 && !settings.getListEmptyFile()) {
					sortedList->doNotListAsDirLastInserted();
				}
			break;
			case FILE_TYPE_RAR:
				strcat(fName, "\\");
				sortedList->insert(curDestPath, fName, buf.nFileSizeHigh * 0x100000000ULL + buf.nFileSizeLow, timeStamp, FILE_TYPE_DIRECTORY);
				strcpy(curPath, fName);
				basePathCnt = strlen(curPath);
				if ((j = ListRAR(str)) < 0) {
					delete sortedList;
					sortedList = NULL;
					return E_EABORTED;
				} else if (j == 0 && !settings.getListEmptyFile()) {
					sortedList->doNotListAsDirLastInserted();
				}
			break;
			case FILE_TYPE_TAR:
			case FILE_TYPE_TGZ:
			case FILE_TYPE_TBZ:
				strcat(fName, "\\");
				sortedList->insert(curDestPath, fName, buf.nFileSizeHigh * 0x100000000ULL + buf.nFileSizeLow, timeStamp, FILE_TYPE_DIRECTORY);
				strcpy(curPath, fName);
				basePathCnt = strlen(curPath);
				if ((j = ListTAR(str, fType)) < 0) {
					delete sortedList;
					sortedList = NULL;
					return E_EABORTED;
				} else if (j == 0 && !settings.getListEmptyFile()) {
					sortedList->doNotListAsDirLastInserted();
				}
			break;
			case FILE_TYPE_ARJ:
				strcat(fName, "\\");
				sortedList->insert(curDestPath, fName, buf.nFileSizeHigh * 0x100000000ULL + buf.nFileSizeLow, timeStamp, FILE_TYPE_DIRECTORY);
				strcpy(curPath, fName);
				basePathCnt = strlen(curPath);
				if ((j = ListARJ(str)) < 0) {
					delete sortedList;
					sortedList = NULL;
					return E_EABORTED;
				} else if (j == 0 && !settings.getListEmptyFile()) {
					sortedList->doNotListAsDirLastInserted();
				}
			break;
			case FILE_TYPE_ACE:
				strcat(fName, "\\");
				sortedList->insert(curDestPath, fName, buf.nFileSizeHigh * 0x100000000ULL + buf.nFileSizeLow, timeStamp, FILE_TYPE_DIRECTORY);
				strcpy(curPath, fName);
				basePathCnt = strlen(curPath);
				if ((j = ListACE(str)) < 0) {
					delete sortedList;
					sortedList = NULL;
					return E_EABORTED;
				} else if (j == 0 && !settings.getListEmptyFile()) {
					sortedList->doNotListAsDirLastInserted();
				}
			break;
			case FILE_TYPE_CAB:
				strcat(fName, "\\");
				sortedList->insert(curDestPath, fName, buf.nFileSizeHigh * 0x100000000ULL + buf.nFileSizeLow, timeStamp, FILE_TYPE_DIRECTORY);
				strcpy(curPath, fName);
				basePathCnt = strlen(curPath);
				if ((j = ListCAB(str)) < 0) {
					delete sortedList;
					sortedList = NULL;
					return E_EABORTED;
				} else if (j == 0 && !settings.getListEmptyFile()) {
					sortedList->doNotListAsDirLastInserted();
				}
			break;
			case FILE_TYPE_ISO:
				strcat(fName, "\\");
				sortedList->insert(curDestPath, fName, buf.nFileSizeHigh * 0x100000000ULL + buf.nFileSizeLow, timeStamp, FILE_TYPE_DIRECTORY);
				strcpy(curPath, fName);
				basePathCnt = strlen(curPath);
				if ((j = ListISO(str)) < 0) {
					delete sortedList;
					sortedList = NULL;
					return E_EABORTED;
				} else if (j == 0 && !settings.getListEmptyFile()) {
					sortedList->doNotListAsDirLastInserted();
				}

			break;
			case FILE_TYPE_REGULAR:
				// do not list regular file
				if (settings.getListOnlyDirectories()) break;
			default:
				sortedList->insert(
					curDestPath,
					fName,
					buf.nFileSizeHigh * 0x100000000ULL + buf.nFileSizeLow,
					timeStamp,
					fType);
				basePathCnt = 0;
		}
		if (progressFunction(fName, buf.nFileSizeLow) == 0) {
			delete sortedList;
			sortedList = NULL;
			return E_EABORTED;
		}

		i += strlen(addList + i) + 1;
	}

	fout = fopen(packedFile, "wt");
	if(fout == NULL) return E_EWRITE;
	if (mainSrcPath[0] != '\0') fprintf(fout, "%s\n", mainSrcPath);
	curPath[0] = '\0';
	sortedList->writeOut(fout, curPath);
	fclose(fout);

	delete sortedList;
	sortedList = NULL;

	return 0;
}

BOOL APIENTRY DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
	switch(reason)
	{
	case DLL_PROCESS_ATTACH:
 		time_t t;
		(void)time(&t);
		struct tm *ts;
		ts = localtime(&t);
		sprintf(todayStr, "%d.%d.%d\t%d:%d.%d",
			ts->tm_year + 1900, ts->tm_mon + 1, ts->tm_mday,
			ts->tm_hour, ts->tm_min, ts->tm_sec);

		settings.setIniLocation(instance);

		settings.readConfig();

		dllInstance = instance;
	}
	return TRUE;
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//------------------ Functions for listing packed files content --------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------

inline void UnixToWindowsDelimiter(char *str)
{
	for(; str[0] != '\0'; str++)
		if(str[0] == '/') str[0] = '\\';
}

//----------------------------------------------------------------------------
// ZIP
// returns number of packed files
// or -1 if aborted by user
int ListZIP(const char* fileName)
{
	unzFile fzip;
	fzip = unzOpen(fileName);
	if(fzip == NULL) return 0;

	int num = 0;
	unz_file_info info;
	char fname[MAX_FULL_PATH_LEN];
	char fullPath[MAX_FULL_PATH_LEN];

	strncpy(fullPath, curPath, basePathCnt);
	fullPath[basePathCnt] = '\0';
	if(unzGoToFirstFile(fzip) != UNZ_OK) {unzClose(fzip); return 0;}
//	FILE*f = fopen("d:\\aaa.txt", "wt");
	do
	{
		if(unzGetCurrentFileInfo(fzip, &info, fname, MAX_FULL_PATH_LEN, NULL, 0, NULL, 0) == UNZ_OK)
		{
			UnixToWindowsDelimiter(fname);
			OemToChar(fname, fname);
			strcpy(fullPath + basePathCnt, fname);
			// > 4GB files incompatible
//			fprintf(f, "\tt->insert(strdup(\"%s\"),0,0,0);\n", fullPath);
			sortedList->insert(curDestPath, fullPath, info.uncompressed_size,
				((info.tmu_date.tm_year - 1980) << 25) |
				((info.tmu_date.tm_mon + 1) << 21) |
				(info.tmu_date.tm_mday << 16) |
				(info.tmu_date.tm_hour << 11) |
				(info.tmu_date.tm_min << 5) | (info.tmu_date.tm_sec / 2),
				fullPath[strlen(fullPath) - 1] == '\\');
			if (progressFunction(NULL, 0) == 0) {
				unzClose(fzip);
				return -1;
			}
			num++;
		}
	}while(unzGoToNextFile(fzip) == UNZ_OK);
	unzClose(fzip);
//	fclose(f);
	return num;
}

//----------------------------------------------------------------------------
// RAR
// returns number of packed files
// or -1 if aborted by user
int ListRAR(const char* fileName)
{
	ArchiveList_struct *list = NULL;
	int num;
	num = urarlib_list((void*)fileName, (ArchiveList_struct*)&list);

	char fullPath[MAX_FULL_PATH_LEN];

	strncpy(fullPath, curPath, basePathCnt);
	fullPath[basePathCnt] = '\0';
	while(list != NULL)
	{
		OemToChar(list->item.Name, list->item.Name);
		strcpy(fullPath + basePathCnt, list->item.Name);
		if((list->item.FileAttr & 16) > 0 && fullPath[strlen(fullPath) - 1] != '\\')
			strcat(fullPath, "\\");
		UnixToWindowsDelimiter(fullPath + basePathCnt);

		// > 4GB files incompatible
		sortedList->insert(curDestPath, fullPath, list->item.UnpSize, list->item.FileTime, (list->item.FileAttr & 16) > 0);
		if (progressFunction(NULL, 0) == 0) {
			urarlib_freelist(list);
			return -1;
		}

		list = (ArchiveList_struct*)list->next;
	}
	urarlib_freelist(list);

	return num;
}

//----------------------------------------------------------------------------
// ACE
// returns number of packed files
// or -1 if interrupted by user
int ListACE(const char* fileName)
{
	// ace header structure
	unsigned short headCRC;
	unsigned short headSize;
	unsigned char  headType;
	unsigned short headFlags;
	unsigned skip;
	unsigned origSize, fTime, attr, crc32, techInfo;
	unsigned short reserved, fNameSize;

	FILE* face;
	int num = 0;

	char fullPath[MAX_FULL_PATH_LEN];

	strncpy(fullPath, curPath, basePathCnt);
	fullPath[basePathCnt] = '\0';

	face = fopen(fileName, "rb");
	if(face == NULL) return 0;

	if(fread(&headCRC, 2, 1, face) != 1 ||
		fread(&headSize, 2, 1, face) != 1 ||
		fread(&headType, 1, 1, face) != 1 ||
		fread(&headFlags, 2, 1, face) != 1 ||
		headType != 0) {fclose(face); return 0;}
	fseek(face, headSize - 3, SEEK_CUR);

	while(TRUE)
	{
		if(fread(&headCRC, 2, 1, face) != 1 ||
			fread(&headSize, 2, 1, face) != 1 ||
			fread(&headType, 1, 1, face) != 1 ||
			fread(&headFlags, 2, 1, face) != 1 ||
			fread(&skip, 4, 1, face) != 1) break;
		if((headFlags & 1) == 0) skip = 0;
		if(headType != 1) // this block is not "file block"
		{
			fseek(face, headSize - 7 + skip, SEEK_CUR);
		}
		else // this is file block
		{
			if(fread(&origSize, 4, 1, face) != 1 ||
				fread(&fTime, 4, 1, face) != 1 ||
				fread(&attr, 4, 1, face) != 1 ||
				fread(&crc32, 4, 1, face) != 1 ||
				fread(&techInfo, 4, 1, face) != 1 ||
				fread(&reserved, 2, 1, face) != 1 ||
				fread(&fNameSize, 2, 1, face) != 1 ||
				fread(fullPath + basePathCnt, 1, fNameSize, face) != fNameSize) break;
			fullPath[basePathCnt + fNameSize] = '\0';
			if((attr & 16) > 0 && fullPath[strlen(fullPath) - 1] != '\\')
				strcat(fullPath, "\\");
			UnixToWindowsDelimiter(fullPath + basePathCnt);
			OemToChar(fullPath + basePathCnt, fullPath + basePathCnt);

			// > 4GB files incompatible
			sortedList->insert(curDestPath, fullPath, origSize, fTime, (attr & 16) > 0);
			if (progressFunction(NULL, 0) == 0) {
				fclose(face);
				return -1;
			}

			num++;
			fseek(face, headSize - 31 - fNameSize + skip, SEEK_CUR);
		}
	}
	fclose(face);

	return num;
}

//----------------------------------------------------------------------------
// ARJ
// returns number of packed files
// or -1 if aborted by user
int ListARJ(const char* fileName)
{
	// arj header relevant fields
	unsigned short headID;
	short headSize;
	unsigned char  first_hdr_size, headType, reserved;
	unsigned fTime, compSize, origSize;

	FILE* farj;
	int num = 0, i, j;

	char fullPath[MAX_FULL_PATH_LEN];

	strncpy(fullPath, curPath, basePathCnt);
	fullPath[basePathCnt] = '\0';

	farj = fopen(fileName, "rb");
	if(farj == NULL) return 0;

	if(fread(&headID, 2, 1, farj) != 1 ||
		headID != 0xEA60 ||
		fread(&headSize, 2, 1, farj) != 1) {fclose(farj); return 0;}
	fseek(farj, 6, SEEK_CUR);
	if(fread(&headType, 1, 1, farj) != 1 || headType != 2) {fclose(farj); return 0;}
	fseek(farj, headSize - 7 + 6, SEEK_CUR);

	while(TRUE)
	{
		if(fread(&headID, 2, 1, farj) != 1 ||
			headID != 0xEA60 ||
			fread(&headSize, 2, 1, farj) != 1 ||
			headSize <= 0 || headSize > 2600 ||
			fread(&first_hdr_size, 1, 1, farj) != 1) break;
		fseek(farj, 5, SEEK_CUR);
		if(fread(&headType, 1, 1, farj) != 1) break;
		if(headType > 3 || headType == 2) // this block is not "file block"
		{
			fseek(farj, headSize - 6 + 6, SEEK_CUR);
		}
		else // this is file block
		{
			if(fread(&reserved, 1, 1, farj) != 1 ||
				fread(&fTime, 4, 1, farj) != 1 ||
				fread(&compSize, 4, 1, farj) != 1 ||
				fread(&origSize, 4, 1, farj) != 1) break;
			fseek(farj, first_hdr_size - 20, SEEK_CUR);
			i = headSize - first_hdr_size;
			j = 0;
			while(i >= 0)
			{
				fread(&reserved, 1, 1, farj);
				fullPath[basePathCnt + j++] = reserved;
				i--;
				if(reserved == 0) break;
			}
			fullPath[basePathCnt + j] = '\0';
			if(headType == 3 && fullPath[strlen(fullPath) - 1] != '\\')
				strcat(fullPath, "\\");
			fseek(farj, i, SEEK_CUR); // skip comment

			UnixToWindowsDelimiter(fullPath + basePathCnt);
			OemToChar(fullPath + basePathCnt, fullPath + basePathCnt);

			// > 4GB files incompatible
			sortedList->insert(curDestPath, fullPath, origSize, fTime, headType == 3);
			if (progressFunction(NULL, 0) == 0) {
				fclose(farj);
				return -1;
			}

			num++;
			fseek(farj, compSize + 6, SEEK_CUR);
		}
	}
	fclose(farj);

	return num;
}

//----------------------------------------------------------------------------
// ISO
// returns number of packed files
// or -1 if interrupted by user
int ListISO(const char* fileName)
{
	int num = 0;
	HANDLE fiso;
	tOpenArchiveData toa;
	toa.ArcName = (char*)fileName; // it suffices for ISO plugin

	fiso = ISO_OpenArchive(&toa);
	if (fiso == 0) return 0;

	char fullPath[MAX_FULL_PATH_LEN];

	strncpy(fullPath, curPath, basePathCnt);

	tHeaderDataEx t;
	int ret;
	while ((ret = ISO_ReadHeaderEx(fiso, &t)) == 0) {
		if (ISO_ProcessFile(fiso) == 0) {
			fullPath[basePathCnt] = '\0';
			strcat(fullPath, t.FileName);
			if((t.FileAttr & 16) == 16 && fullPath[strlen(fullPath) - 1] != '\\')
				strcat(fullPath, "\\");
			UnixToWindowsDelimiter(fullPath + basePathCnt);
			OemToChar(fullPath + basePathCnt, fullPath + basePathCnt);

			// > 4GB files compatible
			sortedList->insert(curDestPath, fullPath, (unsigned)t.UnpSize + _rotl64((unsigned)t.UnpSizeHigh, 32), t.FileTime, (t.FileAttr & 16) == 16);
			if (progressFunction(NULL, 0) == 0) {
				ISO_CloseArchive(fiso);
				return -1;
			}

			num++;
		}
	}

	ISO_CloseArchive(fiso);

	return num;
}

//----------------------------------------------------------------------------
// Listing by WCX - i.e. TotalCommander's plugin
// returns number of packed files
// or -1 if interrupted by user
int ListByWCX(const char* wcx_path, const char* fileName)
{
//				FILE*f=fopen("d:\\aaa.txt", "wt");

	// lets load library and determine its four mandatory functions
    HMODULE hwcx = NULL;
    if(!(hwcx = LoadLibrary(wcx_path))) {
        return 0;
    }
	static fOpenArchive pOpenArchive = NULL;
	static fReadHeader pReadHeader = NULL;
	static fReadHeaderEx pReadHeaderEx = NULL;
	static fProcessFile pProcessFile = NULL;
	static fCloseArchive pCloseArchive = NULL;

	pOpenArchive = (fOpenArchive) GetProcAddress(hwcx, "OpenArchive");
    pReadHeaderEx = (fReadHeaderEx) GetProcAddress(hwcx, "ReadHeaderEx");
	pReadHeader = (fReadHeader) GetProcAddress(hwcx, "ReadHeader");
    pProcessFile = (fProcessFile) GetProcAddress(hwcx, "ProcessFile");
    pCloseArchive = (fCloseArchive) GetProcAddress(hwcx, "CloseArchive");

	// missing one of mandatory functions
    if(!pOpenArchive || !pReadHeader || !pProcessFile || !pCloseArchive) {
		FreeLibrary(hwcx);
		return 0;
	}

	char fullPath[MAX_FULL_PATH_LEN];
	char mainSrcPathBackup[MAX_FULL_PATH_LEN];
	int basePathCnt = ::basePathCnt;
	int listColumnsBackup = settings.getListColumns();

	strncpy(fullPath, curPath, basePathCnt); // pOpenArchive (bellow) can overwrite curPath
	strcpy(mainSrcPathBackup, mainSrcPath); // mainSrcPath can be overwritten as well

	tOpenArchiveData toa;
	toa.ArcName = (char*)fileName;
	toa.OpenMode = PK_OM_LIST;
	HANDLE fwcx = pOpenArchive(&toa);
	if (fwcx == 0) return 0;

	tHeaderData t = {0};
	tHeaderDataEx tex = {0};
	int ret, num = 0;
	if (pReadHeaderEx) { // for TC 7 and above
		notForTC = true;

		settings.setListColumns(1); // listColumns will be determined from file
		while ((ret = pReadHeaderEx(fwcx, &tex)) == 0) {
			if (pProcessFile(fwcx, PK_SKIP, NULL, NULL) == 0) {
				fullPath[basePathCnt] = '\0';
				strcat(fullPath, tex.FileName);
				if((tex.FileAttr & 16) == 16 && fullPath[strlen(fullPath) - 1] != '\\')
					strcat(fullPath, "\\");
				// these two lines are not necessary (the second is even harmfull)
				//UnixToWindowsDelimiter(fullPath + basePathCnt);
				//OemToChar(fullPath + basePathCnt, fullPath + basePathCnt);

				// > 4GB files compatible
				sortedList->insert(curDestPath, fullPath,
					(unsigned)tex.UnpSize + _rotl64((unsigned)tex.UnpSizeHigh, 32),
					tex.FileTime, (tex.FileAttr & 16) == 16);
				if (progressFunction(NULL, 0) == 0) {
					fCloseArchive(fwcx);
					FreeLibrary(hwcx);
					strcpy(mainSrcPath, mainSrcPathBackup); // mainSrcPath can be overwritten
					settings.setListColumns(listColumnsBackup);
					notForTC = false;
					return -1;
				}

				num++;
			}
		}
		strcpy(mainSrcPath, mainSrcPathBackup); // mainSrcPath can be overwritten
		settings.setListColumns(listColumnsBackup);
		notForTC = false;
	} else {
		while ((ret = pReadHeader(fwcx, &t)) == 0) {
			if (pProcessFile(fwcx, PK_SKIP, NULL, NULL) == 0) {
				fullPath[basePathCnt] = '\0';
				strcat(fullPath, t.FileName);
				if((t.FileAttr & 16) == 16 && fullPath[strlen(fullPath) - 1] != '\\')
					strcat(fullPath, "\\");
				// these two lines are not necessary (the second is even harmfull)
				//UnixToWindowsDelimiter(fullPath + basePathCnt);
				//OemToChar(fullPath + basePathCnt, fullPath + basePathCnt);

				// > 4GB files incompatible
				sortedList->insert(curDestPath, fullPath, (unsigned)t.UnpSize, t.FileTime, (t.FileAttr & 16) == 16);
				if (progressFunction(NULL, 0) == 0) {
					fCloseArchive(fwcx);
					FreeLibrary(hwcx);
					return -1;
				}

				num++;
			}
		}
	}

	pCloseArchive(fwcx);
	FreeLibrary(hwcx);

	return num;
}

//----------------------------------------------------------------------------
// CAB
// returns number of packed files
// or -1 if aborted by user
#include <io.h>
#include <fcntl.h>

FNALLOC(mem_alloc) {return malloc(cb);}
FNFREE(mem_free) {free(pv);}
FNOPEN(file_open) {return _open(pszFile, oflag, pmode);}
FNREAD(file_read) {return _read(hf, pv, cb);}
FNWRITE(file_write) {return _write(hf, pv, cb);}
FNCLOSE(file_close) {return _close(hf);}
FNSEEK(file_seek) {return _lseek(hf, dist, seektype);}

FNFDINOTIFY(notification_function)
{
	static char fullPath[MAX_FULL_PATH_LEN];
	switch (fdint)
	{
		case fdintCABINET_INFO: // general information about the cabinet
		case fdintPARTIAL_FILE: // first file in cabinet is continuation
		case fdintNEXT_CABINET:	// file continued to next cabinet
			return 0;

		case fdintCOPY_FILE:	// file to be copied
			strncpy(fullPath, curPath, basePathCnt);
			strcpy(fullPath + basePathCnt, pfdin->psz1);

			sortedList->insert(curDestPath, fullPath, (unsigned)pfdin->cb,
				(((unsigned)pfdin->date) << 16) | pfdin->time, fullPath[strlen(fullPath) - 1] == '\\');
			if (progressFunction(NULL, 0) == 0) {
				return -1;
			}

			return 0; // skip file (i.e. do not extract)

		case fdintCLOSE_FILE_INFO:	// close the file, set relevant info
			return TRUE;
	}

	return 0;
}

int ListCAB(const char* cabinet_fullpath)
{
	HFDI			hfdi;
	ERF				erf;
	FDICABINETINFO	fdici;
	int				hf;
	const char		*p;
	char			cabinet_name[256];
	char			cabinet_path[256];

	hfdi = FDICreate(
		mem_alloc, mem_free,
		file_open, file_read, file_write, file_close, file_seek,
		cpuUNKNOWN, &erf);
	if (hfdi == NULL) return 0;

	hf = file_open((char*)cabinet_fullpath, _O_BINARY | _O_RDONLY | _O_SEQUENTIAL, 0);
	if (hf == -1)
	{
		(void) FDIDestroy(hfdi);
		return 0;
	}

	if (FDIIsCabinet(hfdi, hf, &fdici) == FALSE)
	{
		// this not a cabinet file
		_close(hf);
		(void) FDIDestroy(hfdi);
		return 0;
	}
	else _close(hf);

	p = strrchr(cabinet_fullpath, '\\');

	if (p == NULL)
	{
		strcpy(cabinet_name, cabinet_fullpath);
		strcpy(cabinet_path, "");
	}
	else
	{
		strcpy(cabinet_name, p+1);

		strncpy(cabinet_path, cabinet_fullpath, (int) (p-cabinet_fullpath)+1);
		cabinet_path[ (int) (p-cabinet_fullpath)+1 ] = 0;
	}

	int ret;
	if (TRUE != (ret = FDICopy(
		hfdi,
		cabinet_name, cabinet_path,
		0, notification_function,
		NULL, NULL)))
	{
		(void) FDIDestroy(hfdi);
		return ret;
	}

	(void) FDIDestroy(hfdi);

	return fdici.cFiles;
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//---------------- Functions and dialog boxes for settings -------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------

BOOL CALLBACK DialogFunc(HWND hdwnd, UINT Msg, WPARAM wParam, LPARAM lParam);

void __stdcall ConfigurePacker (HWND parent, HINSTANCE dllInstance)
{
//	MessageBox(parent, "Version 1.0\nLists ACE, ARJ, CAB, JAR, RAR, ZIP\n(c) 2005 Peter Trebatický, Bratislava (Slovakia)\nmailto: peter.trebaticky@gmail.com", "DiskDir Extended", MB_OK);
//	long a = GetWindowThreadProcessId(parent, NULL);
	DialogBox(dllInstance, MAKEINTRESOURCE(IDD_SETTINGS), parent, DialogFunc);
}

BOOL CALLBACK DialogFunc(HWND hdwnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	HWND hList;
	LVITEM lvi;
	hList = GetDlgItem(hdwnd,IDC_LIST_ARCHIVES);
	switch(Msg)
	{
		case WM_INITDIALOG:
			settings.readConfig();

			HICON hiconItem;
			HIMAGELIST hSmall;
			hSmall = ImageList_Create(16, 16, ILC_MASK, 1, 1);
			hiconItem = LoadIcon(dllInstance, MAKEINTRESOURCE(IDI_YES));
			ImageList_AddIcon(hSmall, hiconItem);
			DestroyIcon(hiconItem);
			hiconItem = LoadIcon(dllInstance, MAKEINTRESOURCE(IDI_NO));
			ImageList_AddIcon(hSmall, hiconItem);
			DestroyIcon(hiconItem);
			hiconItem = LoadIcon(dllInstance, MAKEINTRESOURCE(IDI_ASK));
			ImageList_AddIcon(hSmall, hiconItem);
			DestroyIcon(hiconItem);

			ListView_SetImageList(hList, hSmall, LVSIL_SMALL);
			ListView_SetColumnWidth(hList, 0, 65);

			lvi.mask = LVIF_TEXT | LVIF_IMAGE;
			lvi.iSubItem = 0;
			lvi.iItem = 0;
			for (settings.fileTypeMapIt = settings.fileTypeMap.begin(); settings.fileTypeMapIt != settings.fileTypeMap.end(); ++settings.fileTypeMapIt) {
				lvi.iImage = (int) settings.fileTypeMapIt->second.list_this;
				lvi.pszText = (LPSTR) settings.fileTypeMapIt->first.c_str() + 1;
				lvi.cchTextMax = settings.fileTypeMapIt->first.size() - 1;
				ListView_InsertItem(hList, &lvi);
			}

			for (int i = settings.getListColumns(); i >= 1; i--) {
				CheckDlgButton(hdwnd, IDC_RADIO1 + i - 1, BST_CHECKED);
			}

			if (settings.getListArchives()) {
				CheckDlgButton(hdwnd, IDC_CHECK7, BST_CHECKED);
			} else {
				CheckDlgButton(hdwnd, IDC_CHECK7, BST_UNCHECKED);
				EnableWindow(GetDlgItem(hdwnd,IDC_LIST_ARCHIVES),FALSE);
			}

			if (settings.getZerosInMonths()) CheckDlgButton(hdwnd, IDC_CHECK1, BST_CHECKED);
			if (settings.getZerosInDays()) CheckDlgButton(hdwnd, IDC_CHECK2, BST_CHECKED);
			if (settings.getZerosInHours()) CheckDlgButton(hdwnd, IDC_CHECK3, BST_CHECKED);
			if (settings.getZerosInMinutes()) CheckDlgButton(hdwnd, IDC_CHECK4, BST_CHECKED);
			if (settings.getZerosInSeconds()) CheckDlgButton(hdwnd, IDC_CHECK5, BST_CHECKED);

			if (settings.getListOnlyDirectories()) CheckDlgButton(hdwnd, IDC_CHECK6, BST_CHECKED);

/*
			switch (listEmptyFile) {
				case LIST_YES: SetDlgItemText(hdwnd, IDC_EMPTY, "yes"); CheckDlgButton(hdwnd, IDC_EMPTY, BST_CHECKED); break;
				case LIST_NO: SetDlgItemText(hdwnd, IDC_EMPTY, "no"); CheckDlgButton(hdwnd, IDC_EMPTY, BST_UNCHECKED); break;
				case LIST_ASK: SetDlgItemText(hdwnd, IDC_EMPTY, "ask"); CheckDlgButton(hdwnd, IDC_EMPTY, BST_INDETERMINATE); break;
			}
*/
			return TRUE;

		case WM_NOTIFY:
			switch(LOWORD(wParam))
			{
				case IDC_LIST_ARCHIVES:
					if(((LPNMHDR)lParam)->code == NM_RCLICK || ((LPNMHDR)lParam)->code == NM_RDBLCLK) {
						lvi.iItem = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
						if (lvi.iItem != -1) {
							lvi.mask = LVIF_IMAGE;
							lvi.iSubItem = 0;
							ListView_GetItem(hList, &lvi);
							lvi.iImage = (lvi.iImage + 2) % 3;
							ListView_SetItem(hList, &lvi);
						}
					}
					return TRUE;
				default:
					return FALSE;
			}
			break;

		case WM_COMMAND:
			switch(LOWORD(wParam))
			{
				case IDCANCEL:
					EndDialog(hdwnd, 0);
					return TRUE;
				case IDC_EMPTY:
					switch (IsDlgButtonChecked(hdwnd, IDC_EMPTY)) {
						case BST_CHECKED: SetDlgItemText(hdwnd, IDC_EMPTY, "yes"); break;
						case BST_UNCHECKED: SetDlgItemText(hdwnd, IDC_EMPTY, "no"); break;
						case BST_INDETERMINATE: SetDlgItemText(hdwnd, IDC_EMPTY, "ask"); break;
					}
					return TRUE;
				case IDC_CHECK7:
					EnableWindow(GetDlgItem(hdwnd,IDC_LIST_ARCHIVES), IsDlgButtonChecked(hdwnd, IDC_CHECK7));
					return TRUE;
				case IDC_RADIO1:
				case IDC_RADIO2:
				case IDC_RADIO3:
				case IDC_RADIO4:
				case IDC_RADIO5:
				case IDC_RADIO6:
				case IDC_RADIO7:
				case IDC_RADIO8:
					int i;
					for (i = IDC_RADIO1; i <= LOWORD(wParam); i++) CheckDlgButton(hdwnd, i, BST_CHECKED);
					for (; i <= IDC_RADIO8; i++) CheckDlgButton(hdwnd, i, BST_UNCHECKED);
					return TRUE;
				case IDOK:
/*
					switch (IsDlgButtonChecked(hdwnd, IDC_EMPTY)) {
						case BST_UNCHECKED: listEmptyFile = LIST_NO; break;
						case BST_INDETERMINATE: listEmptyFile = LIST_ASK; break;
						case BST_CHECKED: listEmptyFile = LIST_YES; break;
					}
*/
					lvi.iItem = ListView_GetNextItem(hList, -1, LVNI_ALL);
					lvi.iSubItem = 0;
					lvi.mask = LVIF_IMAGE | LVIF_TEXT;
					char text[8192];
					text[0] = '.';
					lvi.pszText = text + 1;
					lvi.cchTextMax = 8190;
					while (lvi.iItem != -1) {
						ListView_GetItem(hList, &lvi);
						if (lvi.pszText != text + 1) strcpy(text + 1, lvi.pszText);
						settings.fileTypeMapIt = settings.fileTypeMap.find(text);
						if (settings.fileTypeMapIt != settings.fileTypeMap.end()) {
							settings.fileTypeMapIt->second.list_this = (LIST_OPTION_ENUM) lvi.iImage;
						}

						lvi.iItem = ListView_GetNextItem(hList, lvi.iItem, LVNI_ALL);
					}
					
					for (i = IDC_RADIO8; i > IDC_RADIO1; i--) if (IsDlgButtonChecked(hdwnd, i) == BST_CHECKED) break;
					settings.setListColumns(i - IDC_RADIO1 + 1);

					settings.setListArchives(IsDlgButtonChecked(hdwnd, IDC_CHECK7));

					settings.setZerosInMonths(IsDlgButtonChecked(hdwnd, IDC_CHECK1) == BST_CHECKED);
					settings.setZerosInDays(IsDlgButtonChecked(hdwnd, IDC_CHECK2) == BST_CHECKED);
					settings.setZerosInHours(IsDlgButtonChecked(hdwnd, IDC_CHECK3) == BST_CHECKED);
					settings.setZerosInMinutes(IsDlgButtonChecked(hdwnd, IDC_CHECK4) == BST_CHECKED);
					settings.setZerosInSeconds(IsDlgButtonChecked(hdwnd, IDC_CHECK5) == BST_CHECKED);

					settings.setListOnlyDirectories(IsDlgButtonChecked(hdwnd, IDC_CHECK6) == BST_CHECKED);
					if(!settings.saveConfig()) {
						char res[8192];
						LoadString(dllInstance, IDS_ERROR_CREATE_INI, (LPSTR) res, 8192);
						MessageBox(hdwnd, res, NULL, MB_OK | MB_ICONERROR);
						return FALSE;
					}
					EndDialog(hdwnd, 0);
					return TRUE;

				case IDC_INFO:
					char msg[8192];
					char res[8192];
					LoadString(dllInstance, IDS_ADDITIONAL_INFO, (LPSTR) res, 8192);
					sprintf_s(msg, 8192, res, settings.getIniFileName());
					LoadString(dllInstance, IDS_ADDITIONAL_INFO_TITLE, (LPSTR) res, 8192);
					MessageBox(hdwnd, msg, res, MB_OK);
					return TRUE;

				default:
					return FALSE;
			}
			break;

		case WM_DESTROY:
			EndDialog(hdwnd, 0);
			return TRUE;
	}
  return FALSE;
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//-------------- Callback function for file list dialog box ------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------

BOOL CALLBACK DialogFunc2(HWND hdwnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	switch(Msg)
	{
	case WM_INITDIALOG:
		SetDlgItemText(hdwnd, IDC_FILE_NAME, (char*)lParam);
		return TRUE;

    case WM_COMMAND:
		switch(LOWORD(wParam))
		{
			case IDOK:
				EndDialog(hdwnd, 0);
				return TRUE;

			case IDCANCEL:
				EndDialog(hdwnd, 1);
				return TRUE;

			case IDC_YES_FORALL:
				EndDialog(hdwnd, 2);
				return TRUE;

			case IDC_NO_FORALL:
				EndDialog(hdwnd, 3);
				return TRUE;

			default:
			  return FALSE;
		}

    case WM_DESTROY:
		EndDialog(hdwnd, 1);
		return TRUE;
  }
  return FALSE;
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------

#define NAME_FIELD_SIZE   100
#define PARAMS(Args) Args
#define CHAR_BIT 8
#define TYPE_SIGNED(t) (! ((t) 0 < (t) -1))
#define TYPE_MINIMUM(t) (TYPE_SIGNED (t) \
			 ? ~ (t) 0 << (sizeof (t) * CHAR_BIT - 1) \
			 : (t) 0)
#define TYPE_MAXIMUM(t) (~ (t) 0 - TYPE_MINIMUM (t))
#define ISODIGIT(c) ((unsigned) (c) - '0' <= 7)

#define TIME_FROM_OCT(where) time_from_oct (where, sizeof (where))
#define OFF_FROM_OCT(where) time_from_oct (where, sizeof (where))

enum read_header
{
  HEADER_STILL_UNREAD,  /* for when read_header has not been called */
  HEADER_SUCCESS,       /* header successfully read and checksummed */
  HEADER_ZERO_BLOCK,    /* zero block where header expected */
  HEADER_END_OF_FILE,   /* true end of file while header expected */
  HEADER_FAILURE        /* ill-formed header, or bad checksum */
};
struct stat current_stat;
union block *current_header;
enum archive_format current_format;
union block *record_start;	/* start of record of archive */
union block *record_end;	/* last+1 block of archive record */
union block *current_block;	/* current block of archive */
int hit_eof;
#define DEFAULT_BLOCKING 20
size_t blocking_factor = DEFAULT_BLOCKING;
size_t record_size = DEFAULT_BLOCKING * BLOCKSIZE;
size_t record_start_block;
char *current_file_name;
char *current_link_name;

unsigned from_oct (char *where, size_t digs)
{
	unsigned value;
	char z = where[digs];
	where[digs] = '\0';
	sscanf(where, "%o", &value);
	where[digs] = z;
	return value;
}

unsigned time_from_oct (char *p, size_t s)
{
	return from_oct (p, s);
}

/*-------------------------------------.
| Perform a read to flush the buffer.  |
`-------------------------------------*/
void flush_read (gzFile ftar, int fType)
{
	size_t status;		/* result from system call */
	size_t left;			/* bytes left */
	char *more;			/* pointer to next byte to read */

error_loop:
	status = (fType == FILE_TYPE_TBZ)
		? BZ2_bzread(ftar, record_start->buffer, record_size)
		: gzread (ftar, record_start->buffer, record_size);
	if (status == record_size)
		return;

	if (status < 0)
		goto error_loop;		/* try again */

	more = record_start->buffer + status;
	left = record_size - status;

	if (left % BLOCKSIZE == 0)
	{
		record_end = record_start + (record_size - left) / BLOCKSIZE;
		return;
	}
//	FATAL_ERROR ((0, 0, _("Only read %lu bytes from archive %s"),
//		(unsigned long) status, *archive_name_cursor));
}

void flush_archive (gzFile ftar, int fType)
{
	record_start_block += record_end - record_start;
	current_block = record_start;
	record_end = record_start + blocking_factor;

	flush_read (ftar, fType);
}

union block * find_next_block (gzFile ftar, int fType)
{
	if (current_block == record_end)
	{
		if (hit_eof)
			return NULL;
		flush_archive (ftar, fType);
		if (current_block == record_end)
		{
			hit_eof = 1;
			return NULL;
		}
	}
	return current_block;
}

void set_next_block_after (union block *block)
{
	while (block >= current_block)
		current_block++;

	/* Do *not* flush the archive here.  If we do, the same argument to
	   set_next_block_after could mean the next block (if the input record
	   is exactly one block long), which is not what is intended.  */

	if (current_block > record_end)
		abort ();
}

size_t available_space_after (union block *pointer)
{
  return record_end->buffer - pointer->buffer;
}

void assign_string (char **string, const char *value)
{
  if (*string)
    free (*string);
  *string = value ? strdup (value) : NULL;
}

/*-----------------------------------------------------------------------.
| Read a block that's supposed to be a header block.  Return its address |
| in "current_header", and if it is good, the file's size in             |
| current_stat.st_size.                                                  |
|                                                                        |
| Return 1 for success, 0 if the checksum is bad, EOF on eof, 2 for a    |
| block full of zeros (EOF marker).                                      |
|                                                                        |
| You must always set_next_block_after(current_header) to skip past the  |
| header which this routine reads.                                       |
`-----------------------------------------------------------------------*/

/* The standard BSD tar sources create the checksum by adding up the
   bytes in the header as type char.  I think the type char was unsigned
   on the PDP-11, but it's signed on the Next and Sun.  It looks like the
   sources to BSD tar were never changed to compute the checksum
   currectly, so both the Sun and Next add the bytes of the header as
   signed chars.  This doesn't cause a problem until you get a file with
   a name containing characters with the high bit set.  So read_header
   computes two checksums -- signed and unsigned.  */
enum read_header read_header(gzFile ftar, int fType)
{
	size_t i;
	long unsigned_sum;		/* the POSIX one :-) */
	long signed_sum;		/* the Sun one :-( */
	long recorded_sum;
	unsigned parsed_sum;
	char *p;
	union block *header;
	char **longp;
	char *bp;
	union block *data_block;
	size_t size, written;
	static char *next_long_name, *next_long_link;

	while (1)
	{
		header = find_next_block (ftar, fType);
		current_header = header;
		if (!header)
			return HEADER_END_OF_FILE;

		parsed_sum = from_oct (header->header.chksum,
			sizeof header->header.chksum);
		if (parsed_sum == (unsigned) -1)
			return HEADER_FAILURE;

		recorded_sum = parsed_sum;
		unsigned_sum = 0;
		signed_sum = 0;
		p = header->buffer;
		for (i = sizeof (*header); i-- != 0;)
		{
			/* We can't use unsigned char here because of old compilers,
			   e.g. V7.  */
			unsigned_sum += 0xFF & *p;
			signed_sum += *p++;
		}

		/* Adjust checksum to count the "chksum" field as blanks.  */
		for (i = sizeof (header->header.chksum); i-- != 0;)
		{
			unsigned_sum -= 0xFF & header->header.chksum[i];
			signed_sum -= header->header.chksum[i];
		}
		unsigned_sum += ' ' * sizeof header->header.chksum;
		signed_sum += ' ' * sizeof header->header.chksum;

		if (unsigned_sum == sizeof header->header.chksum * ' ')
		{
			/* This is a zeroed block...whole block is 0's except for the
			   blanks we faked for the checksum field.  */
			return HEADER_ZERO_BLOCK;
		}

		if (unsigned_sum != recorded_sum && signed_sum != recorded_sum)
			return HEADER_FAILURE;

		/* Good block.  Decode file size and return.  */
		if (header->header.typeflag == LNKTYPE)
			current_stat.st_size = 0;	/* links 0 size on tape */
		else
			current_stat.st_size = OFF_FROM_OCT (header->header.size);

		header->header.name[NAME_FIELD_SIZE - 1] = '\0';
		if (header->header.typeflag == GNUTYPE_LONGNAME ||
		    header->header.typeflag == GNUTYPE_LONGLINK)
		{
			longp = ((header->header.typeflag == GNUTYPE_LONGNAME)
				? &next_long_name
				: &next_long_link);

			set_next_block_after (header);
			if (*longp)
				free (*longp);
			size = current_stat.st_size;
			if (size != current_stat.st_size);
//				FATAL_ERROR ((0, 0, _("Memory exhausted")));
			bp = *longp = (char *) malloc (size);

			for (; size > 0; size -= written)
			{
				data_block = find_next_block (ftar, fType);
				if (data_block == NULL)
				{
//					ERROR ((0, 0, _("Unexpected EOF on archive file")));
					break;
				}
				written = available_space_after (data_block);
				if (written > size)
					written = size;

				memcpy (bp, data_block->buffer, written);
				bp += written;
				set_next_block_after ((union block *) (data_block->buffer + written - 1));
			}
//			free(*longp);
			/* Loop!  */
		}
		else
		{
			char *name = next_long_name;
			struct posix_header *h = &current_header->header;
			char namebuf[sizeof h->prefix + 1 + sizeof h->name + 1];

			if (! name)
			{
				/* Accept file names as specified by POSIX.1-1996
				   section 10.1.1.  */
				char *np = namebuf;
				if (h->prefix[0])
				{
					memcpy (np, h->prefix, sizeof h->prefix);
					np[sizeof h->prefix] = '\0';
					np += strlen (np);
					*np++ = '/';
				}
				memcpy (np, h->name, sizeof h->name);
				np[sizeof h->name] = '\0';
				name = namebuf;
			}

			assign_string (&current_file_name, name);
			assign_string (&current_link_name,
				(next_long_link ? next_long_link
				: current_header->header.linkname));
			next_long_link = next_long_name = 0;
			return HEADER_SUCCESS;
		}
	}
}

void skip_file (gzFile ftar, size_t size, int fType)
{
	union block *x;

	while ((int)size > 0)
	{
		x = find_next_block (ftar, fType);
		if (x == NULL);
//			FATAL_ERROR ((0, 0, _("Unexpected EOF on archive file")));

		set_next_block_after (x);
		size -= BLOCKSIZE;
	}
}

void skip_extended_headers (gzFile ftar, int fType)
{
	union block *exhdr;

	while (1)
	{
		exhdr = find_next_block (ftar, fType);
		if (!exhdr->sparse_header.isextended)
		{
			set_next_block_after (exhdr);
			break;
		}
		set_next_block_after (exhdr);
	}
}

void decode_header (union block *header, struct stat *stat_info,
	enum archive_format *format_pointer, int do_user_group)
{
	enum archive_format format;

	if (strcmp (header->header.magic, TMAGIC) == 0)
		format = POSIX_FORMAT;
	else if (strcmp (header->header.magic, OLDGNU_MAGIC) == 0)
		format = OLDGNU_FORMAT;
	else
		format = V7_FORMAT;
	*format_pointer = format;

	stat_info->st_mode = from_oct(header->header.mode, sizeof(header->header.mode));
	stat_info->st_mtime = TIME_FROM_OCT (header->header.mtime);
}

int list_archive (gzFile ftar, int fType)
{
	int isextended = 0;		/* to remember if current_header is extended */

	decode_header (current_header, &current_stat, &current_format, 0);

	char fullPath[MAX_FULL_PATH_LEN];
	strncpy(fullPath, curPath, basePathCnt);
	fullPath[basePathCnt] = '\0';
	UnixToWindowsDelimiter(current_file_name);
//	OemToChar(current_file_name, current_file_name);
	strcpy(fullPath + basePathCnt, current_file_name);
	if(current_header->header.typeflag == DIRTYPE)
		if(fullPath[strlen(fullPath) - 1] != '\\') strcat(fullPath, "\\");
	struct tm *tm;
	tm = localtime (&(current_stat.st_mtime));
	sortedList->insert(curDestPath, fullPath, (unsigned)current_stat.st_size,
				((tm->tm_year + 1900 - 1980) << 25) |
				((tm->tm_mon + 1) << 21) |
				(tm->tm_mday << 16) |
				(tm->tm_hour << 11) |
				(tm->tm_min << 5) | (tm->tm_sec / 2), current_header->header.typeflag == DIRTYPE ? 1 : 0);

	if (progressFunction(NULL, 0) == 0) return -1;

	/* Check to see if we have an extended header to skip over also.  */
	if (current_header->oldgnu_header.isextended)
		isextended = 1;

	/* Skip past the header in the archive.  */
	set_next_block_after (current_header);

	/* If we needed to skip any extended headers, do so now, by reading
	   extended headers and skipping past them in the archive.  */
	if (isextended)
		skip_extended_headers (ftar, fType);

	/* Skip to the next header on the archive.  */
	skip_file (ftar, current_stat.st_size, fType);

	return 0;
}

/*-----------------------------------.
| Main loop for reading an archive.  |
`-----------------------------------*/
int ListTAR(const char* fileName, int fType)
{
	int num = 0;
	void* ftar; // gzFile == BZFILE* == void*
	ftar = (fType == FILE_TYPE_TBZ) ? BZ2_bzopen(fileName, "rb") : gzopen(fileName, "rb");
	if(ftar == NULL) return 0;

	enum read_header status = HEADER_STILL_UNREAD;
	enum read_header prev_status;

	record_start = (union block *) malloc (record_size);
	if(record_start == NULL) {gzclose(ftar); return 0;}
	current_block = record_start;
	record_end = record_start + blocking_factor;
	record_end = record_start; /* set up for 1st record = # 0 */
	find_next_block (ftar, fType);	/* read it in, check for EOF */
	while (1)
	{
		prev_status = status;
		status = read_header (ftar, fType);
		switch (status)
		{
			case HEADER_STILL_UNREAD:
			break;

			case HEADER_SUCCESS:
				num++;
				/* Valid header.  We should decode next field (mode) first.
				   Ensure incoming names are null terminated.  */

				/* FIXME: This is a quick kludge before 1.12 goes out.  */
				current_stat.st_mtime = TIME_FROM_OCT (current_header->header.mtime);

				if (list_archive(ftar, fType) < 0) {
					free(record_start);
					fType == FILE_TYPE_TBZ ? BZ2_bzclose(ftar) : gzclose(ftar);
					return -1;
				}
				continue;

			case HEADER_ZERO_BLOCK:
			case HEADER_END_OF_FILE:
			break;

			case HEADER_FAILURE:
				set_next_block_after (current_header);
				continue;
		}
		break;
	}

	free(record_start);
	fType == FILE_TYPE_TBZ ? BZ2_bzclose(ftar) : gzclose(ftar);
	return num;
}
