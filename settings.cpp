#include "settings.h"

void Settings::setIniLocation(HINSTANCE instance) {
	// determine, where is this dll (wcx) physically placed
	GetModuleFileName(instance, iniFileName, MAX_FULL_PATH_LEN);
	if(strrchr(iniFileName, '\\') == NULL) iniFileName[0] = '\0';
	else iniFileName[1+(int)strrchr(iniFileName, '\\') - (int)iniFileName] = '\0';
	strcat(iniFileName, "DiskDirExtended.ini");
}

const char* Settings::getLineFormat() {
	if (!lineFormat.empty()) return lineFormat.c_str();
	lineFormat = "%s\t";
	if (listColumns > 1) lineFormat += "%llu";
	if (listColumns > 2) lineFormat += "\t%d";
	if (listColumns > 3) lineFormat += zerosInMonths ? ".%02d" : ".%d";
	if (listColumns > 4) lineFormat += zerosInDays ? ".%02d" : ".%d";
	if (listColumns > 5) lineFormat += zerosInHours ? "\t%02d" : "\t%d";
	if (listColumns > 6) lineFormat += zerosInMinutes ? ":%02d" : ":%d";
	if (listColumns > 7) lineFormat += zerosInSeconds ? ".%02d" : ".%d";
	lineFormat += "\n";
	return lineFormat.c_str();
}

void Settings::readConfig() {
	char buf[8192], *pc, ext[52];
	ENTRY *cur;
	ext[0] = '.'; ext[51] = '\0';
	FILE_TYPE_ELEM fileTypeElem;

	fileTypeElem.which_wcx = wcxmap.end();
	fileTypeElem.list_this = LIST_YES;
	fileTypeMap.clear();
	fileTypeElem.fileType = FILE_TYPE_ACE; fileTypeMap.insert(make_pair(".ace", fileTypeElem));
	fileTypeElem.fileType = FILE_TYPE_ARJ; fileTypeMap.insert(make_pair(".arj", fileTypeElem));
	fileTypeElem.fileType = FILE_TYPE_CAB; fileTypeMap.insert(make_pair(".cab", fileTypeElem));
	fileTypeElem.fileType = FILE_TYPE_JAR; fileTypeMap.insert(make_pair(".jar", fileTypeElem));
	fileTypeElem.fileType = FILE_TYPE_RAR; fileTypeMap.insert(make_pair(".rar", fileTypeElem));
	fileTypeElem.fileType = FILE_TYPE_TAR; fileTypeMap.insert(make_pair(".tar", fileTypeElem));
	fileTypeElem.fileType = FILE_TYPE_TGZ; fileTypeMap.insert(make_pair(".tgz", fileTypeElem));
	fileTypeElem.fileType = FILE_TYPE_TBZ; fileTypeMap.insert(make_pair(".tbz", fileTypeElem));
	fileTypeElem.fileType = FILE_TYPE_TBZ; fileTypeMap.insert(make_pair(".tbz2", fileTypeElem));
	fileTypeElem.fileType = FILE_TYPE_ZIP; fileTypeMap.insert(make_pair(".zip", fileTypeElem));
	fileTypeElem.fileType = FILE_TYPE_ISO; fileTypeMap.insert(make_pair(".iso", fileTypeElem));
	fileTypeElem.fileType = FILE_TYPE_ISO; fileTypeMap.insert(make_pair(".nrg", fileTypeElem));
	fileTypeElem.fileType = FILE_TYPE_ISO; fileTypeMap.insert(make_pair(".bin", fileTypeElem));
	fileTypeElem.fileType = FILE_TYPE_ISO; fileTypeMap.insert(make_pair(".img", fileTypeElem));
	fileTypeElem.fileType = FILE_TYPE_TGZ; fileTypeMap.insert(make_pair(".tar.gz", fileTypeElem));
	fileTypeElem.fileType = FILE_TYPE_TBZ; fileTypeMap.insert(make_pair(".tar.bz", fileTypeElem));
	fileTypeElem.fileType = FILE_TYPE_TBZ; fileTypeMap.insert(make_pair(".tar.bz2", fileTypeElem));

	wcxmap.clear();
	string wincmd_ini_loc = get_wincmd_ini_location();
	CIniFile wincmd_ini;
	if (wincmd_ini.OpenIniFile(wincmd_ini_loc.c_str()) != NULL) {
		cur = wincmd_ini.FindSection("PackerPlugins");
		while (cur != NULL) {
			cur = cur->pNext;
			if (cur == NULL || cur->Type == tpSECTION) break;
			if (cur->Type == tpKEYVALUE) {
				ext[1] = '\0';
				strncpy(buf, cur->pText, 8192);
				pc = strtok(buf, "=");
				if (pc != NULL) {
					strncpy(ext + 1, buf, 50);
					pc = strtok(NULL, ",");
					if (pc != NULL) {
						pc = strtok(NULL, "\n");
						if (pc != NULL) {
						} else ext[1] = '\0';
					} else ext[1] = '\0';
				}
				if (ext[1] != '\0') {
					wcxmap.insert(make_pair(ext, make_pair(pc, false)));
				}
			}
		}
		wincmd_ini.CloseIniFile();
		for (which_wcx = wcxmap.begin(); which_wcx != wcxmap.end(); ++which_wcx) {
			fileTypeElem.fileType = FILE_TYPE_BY_WCX;
			fileTypeElem.which_wcx = which_wcx;
			fileTypeMap.insert(make_pair(which_wcx->first, fileTypeElem));
		}
	}

	tryCanYouHandleThisFile = false;
	wcxHandleableSet.clear();
	listArchives = true;
	listEmptyFile = false;
	equivalent_ext.clear();

	CIniFile my_ini;
	char *str;
	if (my_ini.OpenIniFile(iniFileName) != NULL) {
		str = (char*) my_ini.ReadString("ListingOptions", "ListArchives", "yes");
		listArchives = (strcmp(str, "yes") == 0);
		str = (char*) my_ini.ReadString("ListingOptions", "ListEmptyArchives", "no");
		listEmptyFile = (strcmp(str, "yes") == 0);
		str = (char*) my_ini.ReadString("ListingOptions", "UseCanHandleThisFileForWCXs", "no");
		tryCanYouHandleThisFile = (strcmp(str, "yes") == 0);
		str = (char*) my_ini.ReadString("ListingOptions", "ListOnlyDirectories", "no");
		setListOnlyDirectories(strcmp(str, "yes") == 0);
		setListColumns(my_ini.ReadInt("ListingOptions", "ColumnsToList", 8));
		str = (char*) my_ini.ReadString("ListingOptions", "ZerosInMonths", "no");
		setZerosInMonths(strcmp(str, "yes") == 0);
		str = (char*) my_ini.ReadString("ListingOptions", "ZerosInDays", "no");
		setZerosInDays(strcmp(str, "yes") == 0);
		str = (char*) my_ini.ReadString("ListingOptions", "ZerosInHours", "no");
		setZerosInHours(strcmp(str, "yes") == 0);
		str = (char*) my_ini.ReadString("ListingOptions", "ZerosInMinutes", "no");
		setZerosInMinutes(strcmp(str, "yes") == 0);
		str = (char*) my_ini.ReadString("ListingOptions", "ZerosInSeconds", "no");
		setZerosInSeconds(strcmp(str, "yes") == 0);

		cur = my_ini.FindSection("EquivalentExtensions");
		while (cur != NULL) {
			cur = cur->pNext;
			if (cur == NULL || cur->Type == tpSECTION) break;
			if (cur->Type == tpKEYVALUE) {
				ext[1] = '\0';
				strncpy(buf, cur->pText, 8192); buf[8191] = '\0';
				for (int i = 0; buf[i] != '\0'; i++) buf[i] = tolower(buf[i]);
				equivalent_ext.push_back(buf);
				pc = strtok(buf, "=");
				if (pc != NULL) {
					strncpy(ext + 1, buf, 50);
					fileTypeMapIt = fileTypeMap.find(ext);
					if (fileTypeMapIt != fileTypeMap.end()) {
						pc = strtok(NULL, ",\n");
						while (pc != NULL) {
							strncpy(ext + 1, pc, 50);
							fileTypeElem.fileType = fileTypeMapIt->second.fileType;
							fileTypeElem.which_wcx = fileTypeMapIt->second.which_wcx;
							fileTypeMap.insert(make_pair(ext, fileTypeElem));
							pc = strtok(NULL, ",\n");
						}
					}
				}
			}
		}

		str = (char*) my_ini.ReadString("ListingOptions", "ListAlways", "");
		pc = strtok(str, ",");
		while (pc != NULL) {
			strncpy(ext + 1, pc, 51);
			fileTypeMapIt = fileTypeMap.find(ext);
			if (fileTypeMapIt != fileTypeMap.end()) fileTypeMapIt->second.list_this = LIST_YES;
			pc = strtok(NULL, ",");
		}
		str = (char*) my_ini.ReadString("ListingOptions", "ListNever", "");
		for (int i = 0; str[i] != '\0'; i++) str[i] = tolower(str[i]);
		pc = strtok(str, ",");
		while (pc != NULL) {
			strncpy(ext + 1, pc, 51);
			fileTypeMapIt = fileTypeMap.find(ext);
			if (fileTypeMapIt != fileTypeMap.end()) fileTypeMapIt->second.list_this = LIST_NO;
			pc = strtok(NULL, ",");
		}
		str = (char*) my_ini.ReadString("ListingOptions", "ListAsk", "");
		for (int i = 0; str[i] != '\0'; i++) str[i] = tolower(str[i]);
		pc = strtok(str, ",");
		while (pc != NULL) {
			strncpy(ext + 1, pc, 51);
			fileTypeMapIt = fileTypeMap.find(ext);
			if (fileTypeMapIt != fileTypeMap.end()) fileTypeMapIt->second.list_this = LIST_ASK;
			pc = strtok(NULL, ",");
		}

		str = (char*) my_ini.ReadString("ListingOptions", "ListAlways", "");
		pc = strtok(str, ",");
		while (pc != NULL) {
			strncpy(ext + 1, pc, 51);
			fileTypeMapIt = fileTypeMap.find(ext);
			if (fileTypeMapIt != fileTypeMap.end()) fileTypeMapIt->second.list_this = LIST_YES;
			pc = strtok(NULL, ",");
		}

		str = (char*) my_ini.ReadString("ListingOptions", "UseCanHandleThisFileForWCXsExtensions", "");
		for (int i = 0; str[i] != '\0'; i++) str[i] = tolower(str[i]);
		pc = strtok(str, ",");
		while (pc != NULL) {
			strncpy(ext + 1, pc, 51);
			wcxHandleableSet.insert(ext);
			pc = strtok(NULL, ",");
		}

		my_ini.CloseIniFile();
	}
}

bool Settings::getCanYouHandleThisFile(map <string, pair<string, char> >::iterator it) {
	if (it->second.second == 0) {
		it->second.second = 2 - DetermineCanHandleThisFileCapability(it->second.first.c_str());
	}
	return it->second.second == 1;
}

bool Settings::saveConfig() {
	FILE *fout = fopen(iniFileName, "wt");
	if (fout == NULL) return false;
	fprintf(fout, ";You can edit this file, but be aware that your comments will not be preserved\n");
	fprintf(fout, "[ListingOptions]\n");
	fprintf(fout, ";Whether to list archives, can be \"yes\" or \"no\"\n");
	fprintf(fout, "ListArchives=%s\n", listArchives ? "yes" : "no");
	fprintf(fout, "ListAlways=");
	bool first = true;
	for (fileTypeMapIt = fileTypeMap.begin(); fileTypeMapIt != fileTypeMap.end(); ++fileTypeMapIt) {
		if (fileTypeMapIt->second.list_this == LIST_YES) {
			if (!first) fprintf(fout, ","); else first = false;
			fprintf(fout, "%s", fileTypeMapIt->first.c_str() + 1);
		}
	}
	fprintf(fout, "\n");
	fprintf(fout, "ListNever=");
	first = true;
	for (fileTypeMapIt = fileTypeMap.begin(); fileTypeMapIt != fileTypeMap.end(); ++fileTypeMapIt) {
		if (fileTypeMapIt->second.list_this == LIST_NO) {
			if (!first) fprintf(fout, ","); else first = false;
			fprintf(fout, "%s", fileTypeMapIt->first.c_str() + 1);
		}
	}
	fprintf(fout, "\n");
	fprintf(fout, "ListAsk=");
	first = true;
	for (fileTypeMapIt = fileTypeMap.begin(); fileTypeMapIt != fileTypeMap.end(); ++fileTypeMapIt) {
		if (fileTypeMapIt->second.list_this == LIST_ASK) {
			if (!first) fprintf(fout, ","); else first = false;
			fprintf(fout, "%s", fileTypeMapIt->first.c_str() + 1);
		}
	}
	fprintf(fout, "\n");
	fprintf(fout, ";Whether to list archives that contain no files (resulting in empty directories)\n");
	fprintf(fout, ";can be \"yes\" or \"no\"\n");
	fprintf(fout, "ListEmptyArchives=%s\n", listEmptyFile ? "yes" : "no");
	fprintf(fout, ";Whether to try to list files with unknown extensions by plugins (can be slow)\n");
	fprintf(fout, ";can be \"yes\" or \"no\"\n");
	fprintf(fout, "UseCanHandleThisFileForWCXs=%s\n", tryCanYouHandleThisFile ? "yes" : "no");
	fprintf(fout, ";If UseCanHandleThisFileForWCXs is yes, which extensions to try (empty means all)\n");
	fprintf(fout, "UseCanHandleThisFileForWCXsExtensions=");
	first = true;
	for (wcxHandleableSetIt = wcxHandleableSet.begin(); wcxHandleableSetIt != wcxHandleableSet.end(); ++wcxHandleableSetIt) {
		if (!first) fprintf(fout, ","); else first = false;
		fprintf(fout, "%s", wcxHandleableSetIt->c_str() + 1);
	}
	fprintf(fout, "\n");

	fprintf(fout, ";Whether to list only directories\n");
	fprintf(fout, ";can be \"yes\" or \"no\"\n");
	fprintf(fout, "ListOnlyDirectories=%s\n", listOnlyDirectories ? "yes" : "no");
	fprintf(fout, ";How many columns to list: 1 to 8 inclusive; the order of columns is:\n");
	fprintf(fout, ";  1:name, 2:size, 3:year, 4:month, 5:day, 6:hours, 7:minutes, 8:seconds\n");
	fprintf(fout, "ColumnsToList=%d\n", listColumns);
	fprintf(fout, ";Whether to include leading zeros\n");
	fprintf(fout, ";can be \"yes\" or \"no\"\n");
	fprintf(fout, "ZerosInMonths=%s\n", zerosInMonths ? "yes" : "no");
	fprintf(fout, "ZerosInDays=%s\n", zerosInDays ? "yes" : "no");
	fprintf(fout, "ZerosInHours=%s\n", zerosInHours ? "yes" : "no");
	fprintf(fout, "ZerosInMinutes=%s\n", zerosInMinutes ? "yes" : "no");
	fprintf(fout, "ZerosInSeconds=%s\n", zerosInSeconds ? "yes" : "no");
	fprintf(fout, "\n");
	fprintf(fout, "[EquivalentExtensions]\n");
	fprintf(fout, ";Which extensions are equivalent to the one on the left side of '='\n");
	fprintf(fout, ";for example: zip=war,ear,ip\n");
	for (int i = 0; i < equivalent_ext.size(); i++) {
		fprintf(fout, "%s\n", equivalent_ext[i].c_str());
	}

	fclose(fout);
	return true;
}

bool Settings::DetermineCanHandleThisFileCapability(const char* wcx) {
	// lets load library and determine its functions
	HMODULE hwcx = NULL;
	if(!(hwcx = LoadLibrary(wcx))) {
		return false;
	}

	fGetPackerCaps pGetPackerCaps = NULL;
	fCanYouHandleThisFile pCanYouHandleThisFile = NULL;
	pGetPackerCaps = (fGetPackerCaps) GetProcAddress(hwcx, "GetPackerCaps");
	pCanYouHandleThisFile = (fCanYouHandleThisFile) GetProcAddress(hwcx, "CanYouHandleThisFile");

	// missing one of needed functions
	if(!pGetPackerCaps || !pCanYouHandleThisFile) {
		FreeLibrary(hwcx);
		return false;
	}

	int pc = pGetPackerCaps();
	if ((pc & PK_CAPS_BY_CONTENT) == 0) {
		FreeLibrary(hwcx);
		return false;
	}
	FreeLibrary(hwcx);
	return true;
}

Settings settings;