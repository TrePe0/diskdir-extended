// (c) 2004 Peter Trebaticky
// DiskDir Extended v1.31, plugin for Total Commander, www.ghisler.com
// Last change: 2004/02/22

#include "wcxhead.h"
#include "defs.h"
#include "SortedList.h"
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

#include "resource.h"
#include "resrc1.h"

SortedList sortedList;
// for ReadHeader+ProcessFile as well as PackFiles
char curPath[MAX_FULL_PATH_LEN];
int curPathLen;

char curFileName[MAX_PATH];
char mainSrcPath[MAX_PATH]; // first line of .lst file
tProcessDataProc progressFunction;
tChangeVolProc changeVolFunction;
char todayStr[20]; // "%d.%d.%d\t%d:%d.%d", year, month, day, hour, minute, second
int basePathCnt = 0;
char iniFileName[MAX_PATH];
HINSTANCE dllInstance;

FILE* fout; // file being created
int ListZIP(const char* fileName);
int ListRAR(const char* fileName);
int ListACE(const char* fileName);
int ListARJ(const char* fileName);
int ListCAB(const char* fileName);
int ListTAR(const char* fileName, int fType);

enum { LIST_YES, LIST_NO, LIST_ASK };
int listThisType[FILE_TYPE_LAST];

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
			return f;
		break;
		default:
			archiveData->OpenResult = E_NOT_SUPPORTED;
			return 0;
	}
}

int __stdcall ReadHeader (HANDLE handle, tHeaderData *headerData)
{
	char str[MAX_PATH + 100];
	char *retval;

again:
	retval = fgets(str, MAX_PATH + 100, (FILE*)handle);
	while(retval != NULL && strchr(str, '\t') == NULL) 
	{
		strncpy(mainSrcPath, str, MAX_PATH); mainSrcPath[MAX_PATH - 1] = '\0';
		if(mainSrcPath[strlen(mainSrcPath) - 1] == '\n') 
			mainSrcPath[strlen(mainSrcPath) - 1] = '\0';
		if(mainSrcPath[strlen(mainSrcPath) - 1] != '\\')
			strcat(mainSrcPath, "\\");
		retval = fgets(str, MAX_PATH + 100, (FILE*)handle);
	}

	if(retval != NULL)
	{
		int i, year, month, day, hour, minute, second;

		i = (int)strchr(str, '\t') - (int)str;
		if(str[i - 1] != '\\' && i != 0) // if not directory or separator
		{
			// prepend current directory
			strcpy(headerData->FileName, curPath);
			strncat(headerData->FileName, str, i);
			headerData->FileName[strlen(curPath) + i] = '\0';
		}
		else
		{
			strncpy(headerData->FileName, str, i);
			headerData->FileName[i] = '\0';
		}
		sscanf(str + i, "%d\t%d.%d.%d\t%d:%d.%d", &(headerData->UnpSize),
			&year, &month, &day,
			&hour, &minute, &second);
		headerData->FileTime = 
			((year - 1980) << 25) | (month << 21) | (day << 16) | 
			(hour << 11) | (minute << 5) | (second/2);

		if(str[i - 1] == '\\' || i == 0)
		{
			strcpy(curPath, headerData->FileName);
			curPathLen = strlen(curPath);
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
	}
	return retval == NULL ? E_END_ARCHIVE : 0;
}

int __stdcall ProcessFile (HANDLE handle, int operation, char *destPath, char *destName)
{
	static char wholeNameSrc[MAX_FULL_PATH_LEN];
	static char wholeNameDest[MAX_FULL_PATH_LEN];
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
			i = fread(wholeNameDest, 1, MAX_FULL_PATH_LEN, fin);
			while(i > 0)
			{
				fwrite(wholeNameDest, i, 1, fout);
				progressFunction(wholeNameSrc, i);
				i = fread(wholeNameDest, 1, MAX_FULL_PATH_LEN, fin);
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
	return PK_CAPS_NEW | PK_CAPS_MULTIPLE | PK_CAPS_OPTIONS;
}

/*
  Appropriately writes given file to fout using curPath variable
      fName: name of file including path (ends with '\\' if it is directory)
             path is relative to srcPath (obtained from PackFiles function)
			 path has to contain '\\' as delimiters (not '/')
      fSize: size of the file
  fDateTime: timestamp in MSDOS format
*/
void WriteFileToFout(const char* fName, const unsigned fSize, const unsigned fDateTime)
{
	int year = (fDateTime >> 25) + 1980;
	int month = (fDateTime & 0x1FFFFFF) >> 21;
	int day = (fDateTime & 0x1FFFFF) >> 16;
	int hour = (fDateTime & 0xFFFF) >> 11;
	int minute = (fDateTime & 0x7FF) >> 5;
	int second = (fDateTime & 0x1F) * 2;
	int fNameLen = strlen(fName);
	bool isDir = (fName[fNameLen - 1] == '\\');

	if(strncmp(fName, curPath, curPathLen) == 0)
	{
		if(isDir)
		{
			fprintf(fout, "%s", fName);
			strcpy(curPath, fName); curPathLen = strlen(curPath);
		}
		else
		{
			if(strrchr(fName, '\\') != NULL)
			{
				int j;
				j = (int)strrchr(fName, '\\') - (int)fName + 1;
				if(j > curPathLen)
				{
					strncpy(curPath, fName, j);
					curPath[j] = '\0';
					curPathLen = strlen(curPath);
					fprintf(fout, "%s\t0\t%d.%d.%d\t%d:%d.%d\n", curPath, 
						year, month, day, hour, minute, second);
				}
			}

			fprintf(fout, "%s", fName + curPathLen);
		}
	}
	else
	{
		if(isDir)
		{
			fprintf(fout, "%s", fName);
			strcpy(curPath, fName); curPathLen = strlen(curPath);
		}
		else
		{
			if(strrchr(fName, '\\') != NULL)
			{
				int j;
				j = (int)strrchr(fName, '\\') - (int)fName + 1;
				strncpy(curPath, fName, j);
				curPath[j] = '\0';
				curPathLen = strlen(curPath);
			}
			else
			{
				curPath[0] = '\0'; curPathLen = 0;
			}
			fprintf(fout, "%s\t0\t%d.%d.%d\t%d:%d.%d\n", curPath,
				year, month, day, hour, minute, second);
			fprintf(fout, "%s", fName + curPathLen);
		}
	}

	fprintf(fout, "\t%d\t%d.%d.%d\t%d:%d.%d\n",
		fSize,
		year, month, day,
		hour, minute, second);
}

int DetermineFileType(const char *fName)
{
	if(fName[strlen(fName) - 1] == '\\') return FILE_TYPE_DIRECTORY;

	int i;
	for(i = strlen(fName) - 1; i >= 0 && fName[i] != '.'; i--);
	if(i < 0) return FILE_TYPE_REGULAR;
	else if(stricmp(fName + i, ".ace") == 0) return FILE_TYPE_ACE;
	else if(stricmp(fName + i, ".arj") == 0) return FILE_TYPE_ARJ;
	else if(stricmp(fName + i, ".cab") == 0) return FILE_TYPE_CAB;
	else if(stricmp(fName + i, ".jar") == 0) return FILE_TYPE_JAR;
	else if(stricmp(fName + i, ".rar") == 0) return FILE_TYPE_RAR;
	else if(stricmp(fName + i, ".tar") == 0) return FILE_TYPE_TAR;
	else if(stricmp(fName + i, ".tgz") == 0) return FILE_TYPE_TGZ;
	else if(stricmp(fName + i, ".tbz") == 0) return FILE_TYPE_TBZ;
	else if(stricmp(fName + i, ".tbz2") == 0) return FILE_TYPE_TBZ;
	else if(stricmp(fName + i, ".zip") == 0) return FILE_TYPE_ZIP;
	else if(stricmp(fName + i, ".gz") == 0)
	{
		for(i--; i >= 0 && fName[i] != '.'; i--);
		if(i < 0) return FILE_TYPE_REGULAR;
		if(stricmp(fName + i, ".tar.gz") == 0) return FILE_TYPE_TGZ;
	}
	else if(stricmp(fName + i, ".bz") == 0 || stricmp(fName + i, ".bz2") == 0)
	{
		for(i--; i >= 0 && fName[i] != '.'; i--);
		if(i < 0) return FILE_TYPE_REGULAR;
		if(stricmp(fName + i, ".tar.bz") == 0 ||
		   stricmp(fName + i, ".tar.bz2") == 0) return FILE_TYPE_TBZ;
	}
	return FILE_TYPE_REGULAR;
}

void ReadIniFile()
{
#define SET_LIST_THIS_TYPE_FROM_STR(t, txt) \
	if(strncmp(r, txt, 8) == 0) {\
		if(strncmp(r + 8, "yes", 3) == 0) listThisType[t] = LIST_YES;\
		if(strncmp(r + 8, "no", 2) == 0) listThisType[t] = LIST_NO;\
		if(strncmp(r + 8, "ask", 3) == 0) listThisType[t] = LIST_ASK; }
	int i;
	FILE* fini;
	char r[20];
	for(i = 0; i < FILE_TYPE_LAST; i++) listThisType[i] = LIST_YES;
	fini = fopen(iniFileName, "rt");
	if(fini == NULL) return;

	while(fgets(r, 20, fini) != NULL)
	{
		if(r[strlen(r) - 1] != '\n') // we didn't read the whole line
		{
			while(fscanf(fini, "%c", r + 19) == 1 && r[19] != '\n'); // so read it
			r[19] = '\0';
		}
		SET_LIST_THIS_TYPE_FROM_STR(FILE_TYPE_ACE, "ListACE=") else
		SET_LIST_THIS_TYPE_FROM_STR(FILE_TYPE_ARJ, "ListARJ=") else
		SET_LIST_THIS_TYPE_FROM_STR(FILE_TYPE_CAB, "ListCAB=") else
		SET_LIST_THIS_TYPE_FROM_STR(FILE_TYPE_JAR, "ListJAR=") else
		SET_LIST_THIS_TYPE_FROM_STR(FILE_TYPE_RAR, "ListRAR=") else
		SET_LIST_THIS_TYPE_FROM_STR(FILE_TYPE_TAR, "ListTAR=") else
		SET_LIST_THIS_TYPE_FROM_STR(FILE_TYPE_TBZ, "ListTBZ=") else
		SET_LIST_THIS_TYPE_FROM_STR(FILE_TYPE_TGZ, "ListTGZ=") else
		SET_LIST_THIS_TYPE_FROM_STR(FILE_TYPE_ZIP, "ListZIP=");
	}
	fclose(fini);
}

BOOL CALLBACK DialogFunc2(HWND hdwnd, UINT Msg, WPARAM wParam, LPARAM lParam);

int __stdcall PackFiles(char *packedFile, char *subPath, char *srcPath, char *addList, int flags)
{
	int i;
	char str[MAX_FULL_PATH_LEN];
	char fName[MAX_FULL_PATH_LEN];
	int fType;
	unsigned timeStamp;

	fout = fopen(packedFile, "wt");
	if(fout == NULL) return E_ECREATE;
	
	if(!sortedList.Create()) sortedList.Clear();
	ReadIniFile();

	fprintf(fout, "%s\n", srcPath);
	if(subPath != NULL)
	{
		time_t t;
		fprintf(fout, "%s", subPath);
		if(subPath[strlen(subPath) - 1] != '\\') fprintf(fout, "\\");
		(void)time(&t);
		struct tm *ts;
		ts = localtime(&t);
		fprintf(fout, "\t0\t%s\n", todayStr);
	}

	curPath[0] = '\0'; curPathLen = 0; i = 0;
	while(addList[i] != '\0')
	{
		strcpy(str, srcPath);
		if(str[strlen(str) - 1] != '\\') strcat(str, "\\");
		strcat(str, addList + i);

		WIN32_FILE_ATTRIBUTE_DATA buf;
		FILETIME t;
		SYSTEMTIME ts;

		fType = DetermineFileType(str);
		if(fType == FILE_TYPE_DIRECTORY) str[strlen(str) - 1] = '\0';
		GetFileAttributesEx(str, GetFileExInfoStandard, (LPVOID)(&buf));
		if(fType == FILE_TYPE_DIRECTORY)
		{
			FileTimeToLocalFileTime(&(buf.ftCreationTime), &t);
			strcat(str, "\\");
			buf.nFileSizeLow = 0;
		}
		else FileTimeToLocalFileTime(&(buf.ftLastWriteTime), &t);
		FileTimeToSystemTime(&t, &ts);

		timeStamp = ((ts.wYear - 1980) << 25) | (ts.wMonth << 21) | (ts.wDay << 16) | 
					(ts.wHour << 11) | (ts.wMinute << 5) | (ts.wSecond / 2);

		strcpy(fName, addList + i);
		progressFunction(fName, 0);
		switch(listThisType[fType])
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
						listThisType[fType] = LIST_YES;
					break;
					case 3: // No for all
						listThisType[fType] = LIST_NO;
						fType = FILE_TYPE_REGULAR;
					break;
				}
			break;
			case LIST_YES:
			break;
		}
		switch(fType)
		{
			case FILE_TYPE_ZIP:
			case FILE_TYPE_JAR:
				strcat(fName, "\\");
				sortedList.Insert(fName, buf.nFileSizeLow, timeStamp, FILE_TYPE_DIRECTORY);
				strcpy(curPath, fName);
				basePathCnt = strlen(curPath);
				ListZIP(str);
			break;
			case FILE_TYPE_RAR:
				strcat(fName, "\\");
				sortedList.Insert(fName, buf.nFileSizeLow, timeStamp, FILE_TYPE_DIRECTORY);
				strcpy(curPath, fName);
				basePathCnt = strlen(curPath);
				ListRAR(str);
			break;
			case FILE_TYPE_TAR:
			case FILE_TYPE_TGZ:
			case FILE_TYPE_TBZ:
				strcat(fName, "\\");
				sortedList.Insert(fName, buf.nFileSizeLow, timeStamp, FILE_TYPE_DIRECTORY);
				strcpy(curPath, fName);
				basePathCnt = strlen(curPath);
				ListTAR(str, fType);
			break;
			case FILE_TYPE_ARJ:
				strcat(fName, "\\");
				sortedList.Insert(fName, buf.nFileSizeLow, timeStamp, FILE_TYPE_DIRECTORY);
				strcpy(curPath, fName);
				basePathCnt = strlen(curPath);
				ListARJ(str);
			break;
			case FILE_TYPE_ACE:
				strcat(fName, "\\");
				sortedList.Insert(fName, buf.nFileSizeLow, timeStamp, FILE_TYPE_DIRECTORY);
				strcpy(curPath, fName);
				basePathCnt = strlen(curPath);
				ListACE(str);
			break;
			case FILE_TYPE_CAB:
				strcat(fName, "\\");
				sortedList.Insert(fName, buf.nFileSizeLow, timeStamp, FILE_TYPE_DIRECTORY);
				strcpy(curPath, fName);
				basePathCnt = strlen(curPath);
				ListCAB(str);
			break;
			default:
				sortedList.Insert(fName, buf.nFileSizeLow, timeStamp, fType);
				basePathCnt = 0;
		}
		progressFunction(fName, buf.nFileSizeLow);

		i += strlen(addList + i) + 1;
	}


	for(i = sortedList.GetFirst(); i != -1; i = sortedList.GetList()[i].next)
		WriteFileToFout(sortedList.GetList()[i].name, sortedList.GetList()[i].size, sortedList.GetList()[i].time);

	fclose(fout);
	sortedList.Destroy();
	return 0;
}

BOOL APIENTRY DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
	switch(reason)
	{
	case DLL_PROCESS_ATTACH:
	case DLL_THREAD_ATTACH:
		time_t t;
		(void)time(&t);
		struct tm *ts;
		ts = localtime(&t);
		sprintf(todayStr, "%d.%d.%d\t%d:%d.%d",
			ts->tm_year + 1900, ts->tm_mon + 1, ts->tm_mday,
			ts->tm_hour, ts->tm_min, ts->tm_sec);

		// determine, where is this dll (wcx) physically placed
		GetModuleFileName(instance, iniFileName, MAX_PATH);
		if(strrchr(iniFileName, '\\') == NULL) iniFileName[0] = '\0';
		else iniFileName[1+(int)strrchr(iniFileName, '\\') - (int)iniFileName] = '\0';

		strcat(iniFileName, "DiskDirExtended.ini");
	}
	dllInstance = instance;
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
	do
	{
		if(unzGetCurrentFileInfo(fzip, &info, fname, MAX_FULL_PATH_LEN, NULL, 0, NULL, 0) == UNZ_OK)
		{
			UnixToWindowsDelimiter(fname);
			OemToChar(fname, fname);
			strcpy(fullPath + basePathCnt, fname);
			sortedList.Insert(fullPath, info.uncompressed_size,
				((info.tmu_date.tm_year - 1980) << 25) |
				((info.tmu_date.tm_mon + 1) << 21) |
				(info.tmu_date.tm_mday << 16) | 
				(info.tmu_date.tm_hour << 11) |
				(info.tmu_date.tm_min << 5) | (info.tmu_date.tm_sec / 2), 
				fullPath[strlen(fullPath) - 1] == '\\');
			num++;
		}
	}while(unzGoToNextFile(fzip) == UNZ_OK);
	unzClose(fzip);
	
	return num;
}

//----------------------------------------------------------------------------
// RAR
// returns number of packed files
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

		sortedList.Insert(fullPath, list->item.UnpSize, list->item.FileTime, (list->item.FileAttr & 16) > 0);

		list = (ArchiveList_struct*)list->next;
	}
	urarlib_freelist(list);
	
	return num;
}

//----------------------------------------------------------------------------
// ACE
// returns number of packed files
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

			sortedList.Insert(fullPath, origSize, fTime, (attr & 16) > 0);

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

			sortedList.Insert(fullPath, origSize, fTime, headType == 3);

			num++;
			fseek(farj, compSize + 6, SEEK_CUR);
		}
	}
	fclose(farj);

	return num;
}

//----------------------------------------------------------------------------
// CAB
// returns number of packed files
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

			sortedList.Insert(fullPath, pfdin->cb,
				(((unsigned)pfdin->date) << 16) | pfdin->time, fullPath[strlen(fullPath) - 1] == '\\');

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
	char			*p;
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

	if (TRUE != FDICopy(
		hfdi,
		cabinet_name, cabinet_path,
		0, notification_function,
		NULL, NULL))
	{
		(void) FDIDestroy(hfdi);
		return 0;
	}

	(void) FDIDestroy(hfdi);

	return fdici.cFiles;
}

/*
// TAR
// returns number of packed files
int ListTAR(FILE *fout, const char* fileName)
{
	// tar header structure (just relevant fields)
	char fname[100], mode[8], uid[8], gid[8], size[12], mtime[12];
	unsigned fSize, fTime;
	FILE* ftar;
	int num = 0;

	char curTarPath[MAX_FULL_PATH_LEN];

	ftar = fopen(fileName, "rb");
	if(ftar == NULL) return 0;

	curTarPath[0] = '\0';
	while(TRUE)
	{
		if(fread(fname, 1, 100, ftar) != 100 ||
			fread(mode, 1, 8, ftar) != 8 ||
			fread(uid, 1, 8, ftar) != 8 ||
			fread(gid, 1, 8, ftar) != 8 ||
			fread(size, 1, 12, ftar) != 12 ||
			fread(mtime, 1, 12, ftar) != 12) break;
		if(sscanf(size, "%o", &fSize) != 1) break;
		if(sscanf(mtime, "%o", &fTime) != 1) break;
		fseek(ftar, 364 + ((fSize + 511) / 512) * 512, SEEK_CUR);

		UnixToWindowsDelimiter(fname);

		if(strncmp(fname, curTarPath, strlen(curTarPath)) == 0)
		{
			if(fname[strlen(fname) - 1] == '\\')
			{
				if(fname[strlen(fname) - 1] != '\\') strcat(fname, "\\");
				fprintf(fout, "%s%s", curPath, fname);
				strcpy(curTarPath, fname);
			}
			else
			{
				fprintf(fout, "%s", fname + strlen(curTarPath));
			}
		}
		else
		{
			if(fname[strlen(fname) - 1] == '\\')
			{
				if(fname[strlen(fname) - 1] != '\\') strcat(fname, "\\");
				fprintf(fout, "%s%s", curPath, fname);
				strcpy(curTarPath, fname);
			}
			else
			{
				if(strrchr(fname, '\\') != NULL)
				{
					int j;
					j = (int)strrchr(fname, '\\') - (int)fname + 1;
					strncpy(curTarPath, fname, j);
					curTarPath[j] = '\0';
				}
				else
				{
					curTarPath[0] = '\0';
				}
				fprintf(fout, "%s%s\t0\t1980.1.1\t1:0.0\n", curPath, curTarPath);
				fprintf(fout, "%s", fname + strlen(curTarPath));
			}
		}
		struct tm *ts;
		ts = localtime((time_t*)&fTime);
		fprintf(fout, "\t%d\t%d.%d.%d\t%d:%d.%d\n",
			fSize,
			ts->tm_year + 1900, ts->tm_mon + 1, ts->tm_mday,
			ts->tm_hour, ts->tm_min, ts->tm_sec);

		num++;
	}
	fclose(ftar);

	return num;
}
*/

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//---------------- Functions and dialog boxes for settings -------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------

BOOL CALLBACK DialogFunc(HWND hdwnd, UINT Msg, WPARAM wParam, LPARAM lParam);

void __stdcall ConfigurePacker (HWND parent, HINSTANCE dllInstance)
{
//	MessageBox(parent, "Version 1.0\nLists ACE, ARJ, CAB, JAR, RAR, ZIP\n(c) 2004 Peter Trebatický, Bratislava (Slovakia)\nmailto: trepe@szm.sk", "DiskDir Extended", MB_OK);
//	long a = GetWindowThreadProcessId(parent, NULL);
	DialogBox(dllInstance, MAKEINTRESOURCE(IDD_SETTINGS), parent, DialogFunc);
}

BOOL CALLBACK DialogFunc(HWND hdwnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
#define SET_STATE_FROM_STR(id, txt) \
	if(strncmp(r, txt, 8) == 0) {\
		if(strncmp(r + 8, "yes", 3) == 0) CheckDlgButton(hdwnd, id, BST_UNCHECKED);\
		if(strncmp(r + 8, "no", 2) == 0) CheckDlgButton(hdwnd, id, BST_INDETERMINATE);\
		if(strncmp(r + 8, "ask", 3) == 0) CheckDlgButton(hdwnd, id, BST_CHECKED); }
#define UPDATE_STATE(id) \
	switch(IsDlgButtonChecked(hdwnd, id)) {\
		case BST_UNCHECKED: SetDlgItemText(hdwnd, id, "yes"); break;\
		case BST_INDETERMINATE: SetDlgItemText(hdwnd, id, "no"); break;\
		case BST_CHECKED: SetDlgItemText(hdwnd, id, "ask"); break; }
#define WRITE_STATE(id, txt) \
	fprintf(fout, txt);\
	switch(IsDlgButtonChecked(hdwnd, id)) {\
		case BST_UNCHECKED: fprintf(fout, "yes"); break;\
		case BST_INDETERMINATE: fprintf(fout, "no"); break;\
		case BST_CHECKED: fprintf(fout, "ask"); break; }

	switch(Msg)
	{
	case WM_INITDIALOG:
		char r[100];
		CheckDlgButton(hdwnd, IDC_ACE, BST_UNCHECKED);
		CheckDlgButton(hdwnd, IDC_ARJ, BST_UNCHECKED);
		CheckDlgButton(hdwnd, IDC_CAB, BST_UNCHECKED);
		CheckDlgButton(hdwnd, IDC_JAR, BST_UNCHECKED);
		CheckDlgButton(hdwnd, IDC_RAR, BST_UNCHECKED);
		CheckDlgButton(hdwnd, IDC_TAR, BST_UNCHECKED);
		CheckDlgButton(hdwnd, IDC_TBZ, BST_UNCHECKED);
		CheckDlgButton(hdwnd, IDC_TGZ, BST_UNCHECKED);
		CheckDlgButton(hdwnd, IDC_ZIP, BST_UNCHECKED);

		FILE* fin;
		fin = fopen(iniFileName, "rt");
		if(fin == NULL) return TRUE;
		while(fgets(r, 100, fin) != NULL)
		{
			if(r[strlen(r) - 1] != '\n') // we didn't read the whole line
			{
				while(fscanf(fin, "%c", r + 99) == 1 && r[99] != '\n'); // so read it
				r[99] = '\0';
			}
			SET_STATE_FROM_STR(IDC_ACE, "ListACE=") else
			SET_STATE_FROM_STR(IDC_ARJ, "ListARJ=") else
			SET_STATE_FROM_STR(IDC_CAB, "ListCAB=") else
			SET_STATE_FROM_STR(IDC_JAR, "ListJAR=") else
			SET_STATE_FROM_STR(IDC_RAR, "ListRAR=") else
			SET_STATE_FROM_STR(IDC_TAR, "ListTAR=") else
			SET_STATE_FROM_STR(IDC_TBZ, "ListTBZ=") else
			SET_STATE_FROM_STR(IDC_TGZ, "ListTGZ=") else
			SET_STATE_FROM_STR(IDC_ZIP, "ListZIP=");
		}
		fclose(fin);
		UPDATE_STATE(IDC_ACE);
		UPDATE_STATE(IDC_ARJ);
		UPDATE_STATE(IDC_CAB);
		UPDATE_STATE(IDC_JAR);
		UPDATE_STATE(IDC_RAR);
		UPDATE_STATE(IDC_TAR);
		UPDATE_STATE(IDC_TBZ);
		UPDATE_STATE(IDC_TGZ);
		UPDATE_STATE(IDC_ZIP);

		return TRUE;

    case WM_COMMAND:
		switch(LOWORD(wParam))
		{
			case IDCANCEL:
				EndDialog(hdwnd, 0);
				return TRUE;

			case IDOK:
				FILE* fout;
				fout = fopen(iniFileName, "wt");
				if(fout == NULL)
				{
					MessageBox(hdwnd, "Unable to create ini file", NULL, MB_OK | MB_ICONERROR);
					return FALSE;
				}
				WRITE_STATE(IDC_ACE, "ListACE=");
				WRITE_STATE(IDC_ARJ, "\nListARJ=");
				WRITE_STATE(IDC_CAB, "\nListCAB=");
				WRITE_STATE(IDC_JAR, "\nListJAR=");
				WRITE_STATE(IDC_RAR, "\nListRAR=");
				WRITE_STATE(IDC_TAR, "\nListTAR=");
				WRITE_STATE(IDC_TBZ, "\nListTBZ=");
				WRITE_STATE(IDC_TGZ, "\nListTGZ=");
				WRITE_STATE(IDC_ZIP, "\nListZIP=");
				fprintf(fout, "\n");
				fclose(fout);

				EndDialog(hdwnd, 0);
				return TRUE;

			case IDC_ACE: UPDATE_STATE(IDC_ACE); return TRUE; break;
			case IDC_ARJ: UPDATE_STATE(IDC_ARJ); return TRUE; break;
			case IDC_CAB: UPDATE_STATE(IDC_CAB); return TRUE; break;
			case IDC_JAR: UPDATE_STATE(IDC_JAR); return TRUE; break;
			case IDC_RAR: UPDATE_STATE(IDC_RAR); return TRUE; break;
			case IDC_TAR: UPDATE_STATE(IDC_TAR); return TRUE; break;
			case IDC_TBZ: UPDATE_STATE(IDC_TBZ); return TRUE; break;
			case IDC_TGZ: UPDATE_STATE(IDC_TGZ); return TRUE; break;
			case IDC_ZIP: UPDATE_STATE(IDC_ZIP); return TRUE; break;

			case IDC_INFO: 
				MessageBox(hdwnd, 
					"This plugin is free software. It uses:\n"
					"- minizip (c) 1998-2003 Gilles Vollant\n"
					"    from zlib 1.2.1 (c) 1995-2003 Jean-loup Gailly and Mark Adler\n"
					"- UniquE RAR File Library 0.4.0 (c) 2000-2002 by Christian Scheurer\n"
					"- Cabinet SDK (c) 1993-1997 Microsoft Corporation\n"
					"- parts from GNU tar 1.13 (c) 1994-1999 Free Software Foundation, Inc.\n"
					"- libbzip2 1.0.2 (c) 1996-2002 Julian R Seward\n"
 					"- UPX 1.24w (c) 1996-2002 Markus F.X.J. Oberhumer & Laszlo Molnar\n"
					"    for reducing the size of this plugin",
					"DiskDir Extended - Additional Information", MB_OK);
				return TRUE;

			default:
			  return FALSE;
		}

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

void list_archive (gzFile ftar, int fType)
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
	sortedList.Insert(fullPath, current_stat.st_size,
				((tm->tm_year + 1900 - 1980) << 25) |
				((tm->tm_mon + 1) << 21) |
				(tm->tm_mday << 16) |
				(tm->tm_hour << 11) |
				(tm->tm_min << 5) | (tm->tm_sec / 2), current_header->header.typeflag == DIRTYPE ? 1 : 0);

//	progressFunction(fullPath, current_stat.st_size);
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

				list_archive(ftar, fType);
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
