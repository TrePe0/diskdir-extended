/*
1. The highest priority for wincmd.ini locations is the /i commandline parameter.
Use the GetCommandLine Winapi function to get this path and extract the value 
of the /i parameter. If the commandline string contains /i --> done, otherwise continue to 2.
2. Search for the file Wincmd.ini in TC program directory (%COMMANDER_PATH% environment string).
If the file exists continue to 2a. Otherwise continue to 3.
2a. Read UseIniInProgramDir setting in [Configuration] section.
Binary compare UseIniInProgramDir with 1 to find out if wincmd.ini in current directory is used.
If this is the case continue to 2b, otherwise continue to 3.
2b. Binary compare UseIniInProgramDir with 4 to find out of UseIniInProgramDir
should overwrite eventually existing registry values.
If this is the case --> done, otherwise continue to 3.
3. Read the registry setting "IniFileName" in HKEY_CURRENT_USER\Software\Ghisler.
If path has been found --> done, otherwise read the setting "IniFileName" in 
HKEY_LOCAL_MACHINE\Software\Ghisler. If a path has been found here --> done, otherwise continue to 3a.
3a. If 2a returned true (UseIniInProgramDir AND 1) use the Wincmd.ini in program directory (done)
otherwise continue to 4.
4. The lowest priority is a Wincmd.ini file in Windows directory (done).
Otherwise no valid ini file exists. 
*/
#include "inilocator.h"

PCHAR* CommandLineToArgvA(PCHAR CmdLine, int* _argc)
{
	PCHAR* argv;
	PCHAR  _argv;
	ULONG   len;
	ULONG   argc;
	CHAR   a;
	ULONG   i, j;

	BOOLEAN  in_QM;
	BOOLEAN  in_TEXT;
	BOOLEAN  in_SPACE;

	len = strlen(CmdLine);
	i = ((len+2)/2)*sizeof(PVOID) + sizeof(PVOID);

	argv = (PCHAR*)GlobalAlloc(GMEM_FIXED,
		i + (len+2)*sizeof(CHAR));

	_argv = (PCHAR)(((PUCHAR)argv)+i);

	argc = 0;
	argv[argc] = _argv;
	in_QM = FALSE;
	in_TEXT = FALSE;
	in_SPACE = TRUE;
	i = 0;
	j = 0;

	while( a = CmdLine[i] ) {
		if(in_QM) {
			if(a == '\"') {
				in_QM = FALSE;
			} else {
				_argv[j] = a;
				j++;
			}
		} else {
			switch(a) {
				case '\"':
					in_QM = TRUE;
					in_TEXT = TRUE;
					if(in_SPACE) {
						argv[argc] = _argv+j;
						argc++;
					}
					in_SPACE = FALSE;
					break;
				case ' ':
				case '\t':
				case '\n':
				case '\r':
					if(in_TEXT) {
						_argv[j] = '\0';
						j++;
					}
					in_TEXT = FALSE;
					in_SPACE = TRUE;
					break;
				default:
					in_TEXT = TRUE;
					if(in_SPACE) {
						argv[argc] = _argv+j;
						argc++;
					}
					_argv[j] = a;
					j++;
					in_SPACE = FALSE;
					break;
			}
		}
		i++;
	}
	_argv[j] = '\0';
	argv[argc] = NULL;

	(*_argc) = argc;
	return argv;
}

string get_wincmd_ini_location() {
	LPSTR *szArglist;
	int nArgs;
	int i;
	string fname = "wincmd.ini";
	CHAR buf[8192];
	bool name_found = false;

	szArglist = CommandLineToArgvA((PCHAR) GetCommandLineA(), &nArgs);
	if (NULL != szArglist) {
		for (i = 0; i < nArgs; i++) {
			if (szArglist[i][0] == '/' && (szArglist[i][1] == 'i' || szArglist[i][1] == 'I') && szArglist[i][2] == '=') {
				fname = szArglist[i]+3;
				name_found = true; 

				// if name contains backslash, we are done
				for (i = fname.size() - 1; i >= 0; i--) if (fname[i] == '\\') break;
				if (i >= 0) break;

				// if name does not contain backslash, it is the file in %windir%
				ExpandEnvironmentStrings("%windir%\\", buf, 8192);
				fname.insert(0, buf);
				break;
			}
		}
		LocalFree(szArglist);
	}

	if (!name_found) {
		int useIniInProgramDir = 0;
		ExpandEnvironmentStrings("%COMMANDER_PATH%\\", buf, 8192);
		strcat_s(buf, 8192, fname.c_str());
		CIniFile iniFile;

		if (iniFile.OpenIniFile(buf)) {
			useIniInProgramDir = iniFile.ReadInt("Configuration", "UseIniInProgramDir", 0);
			iniFile.CloseIniFile();
		}

		if ((useIniInProgramDir & 1) && (useIniInProgramDir & 4)) {
			fname = buf;
		} else {
			// check registry for IniFileName
			HKEY hkeySOFTWARE = NULL, hkeyGhisler = NULL, hkeyTC = NULL;
			char buf2[8192];
			DWORD buflen = 8192;
			if (RegOpenKeyEx(HKEY_CURRENT_USER, ("SOFTWARE"), 0, KEY_READ, &hkeySOFTWARE) == ERROR_SUCCESS &&
				RegOpenKeyEx(hkeySOFTWARE, ("Ghisler"), 0, KEY_READ, &hkeyGhisler) == ERROR_SUCCESS &&
				RegOpenKeyEx(hkeyGhisler, ("Total Commander"), 0, KEY_READ, &hkeyTC) == ERROR_SUCCESS &&
				RegQueryValueEx(hkeyTC, ("IniFileName"), NULL, NULL, (LPBYTE) buf2, &buflen) == ERROR_SUCCESS) 
			{
				for (i = 0; buf2[i] != '\0'; i++) if (buf2[i] == '\\') break;
				if (buf2[i] == '\0' || (buf2[0] == '.' && buf2[1] == '\\')) {
					if (buf2[i] == '\0') ExpandEnvironmentStrings("%windir%\\", buf, 8192);
					else ExpandEnvironmentStrings("%COMMANDER_PATH%\\", buf, 8192);
					strcat(buf, buf2);
				} else {
					ExpandEnvironmentStrings(buf2, buf, 8192);
				}
				fname = buf;
				name_found = true;
			}
			if (hkeySOFTWARE != NULL) RegCloseKey(hkeySOFTWARE);
			if (hkeyGhisler != NULL) RegCloseKey(hkeyGhisler);
			if (hkeyTC != NULL) RegCloseKey(hkeyTC);
			hkeySOFTWARE = NULL, hkeyGhisler = NULL, hkeyTC = NULL;

			if (!name_found &&
				RegOpenKeyEx(HKEY_LOCAL_MACHINE, ("SOFTWARE"), 0, KEY_READ, &hkeySOFTWARE) == ERROR_SUCCESS &&
				RegOpenKeyEx(hkeySOFTWARE, ("Ghisler"), 0, KEY_READ, &hkeyGhisler) == ERROR_SUCCESS &&
				RegOpenKeyEx(hkeyGhisler, ("Total Commander"), 0, KEY_READ, &hkeyTC) == ERROR_SUCCESS &&
				RegQueryValueEx(hkeyTC, ("IniFileName"), NULL, NULL, (LPBYTE) buf2, &buflen) == ERROR_SUCCESS) 
			{
				for (i = 0; buf2[i] != '\0'; i++) if (buf2[i] == '\\') break;
				if (buf2[i] == '\0' || (buf2[0] == '.' && buf2[1] == '\\')) {
					if (buf2[i] == '\0') ExpandEnvironmentStrings("%windir%\\", buf, 8192);
					else ExpandEnvironmentStrings("%COMMANDER_PATH%\\", buf, 8192);
					strcat(buf, buf2 + 2);
				} else {
					ExpandEnvironmentStrings(buf2, buf, 8192);
				}
				fname = buf;
				name_found = true;
			}
			if (hkeySOFTWARE != NULL) RegCloseKey(hkeySOFTWARE);
			if (hkeyGhisler != NULL) RegCloseKey(hkeyGhisler);
			if (hkeyTC != NULL) RegCloseKey(hkeyTC);

			if (!name_found && (useIniInProgramDir & 1)) {
				fname = buf;
				name_found = true;
			}
			if (!name_found) {
				ExpandEnvironmentStrings("%windir%\\", buf, 8192);
				fname.insert(0, buf);
			}
		}
	}
/*
	FILE *fout = fopen("c:\\ccc.txt", "wt");
	if (fout != NULL) {
		fprintf(fout, "%s\n", fname.c_str());
		ExpandEnvironmentStrings("%COMMANDER_PATH%\\", buf, 8192);
		fprintf(fout, "%s\n", buf);
		ExpandEnvironmentStrings("%USERPROFILE%\\", buf, 8192);
		fprintf(fout, "%s\n", buf);
		fclose(fout);
	}
*/
	return fname;
}
