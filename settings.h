#ifndef __SETTINGS_H
#define __SETTINGS_H

#include "inilocator.h"
#include <string>
#include <map>
#include <vector>
#include <set>
#include "libs/inifile/IniFile.h"
#include "defs.h"
#include "api_totalcmd.h"

using namespace std;

class Settings {
	char iniFileName[MAX_FULL_PATH_LEN];

	vector <string> equivalent_ext; // as lines from DiskDirExtended.ini

	string lineFormat;

	bool listArchives;
	bool tryCanYouHandleThisFile;
	bool listEmptyFile;
	bool listOnlyDirectories;

	// how many columns to list: 1..8 = (name, size, years, months, days, hours, minutes, seconds)
	int listColumns;
	bool zerosInDays;
	bool zerosInMonths;
	bool zerosInHours;
	bool zerosInMinutes;
	bool zerosInSeconds;

	bool DetermineCanHandleThisFileCapability(const char*);

public:
	map <string, pair<string, bool> > wcxmap; // mapping extension -> (wcx, can use CanYouHandleThisFile)
	map <string, pair<string, bool> >::iterator which_wcx; // iterator for wcxmap
	map <string, FILE_TYPE_ELEM> fileTypeMap;
	map <string, FILE_TYPE_ELEM>::iterator fileTypeMapIt;
	set <string> wcxHandleableSet;
	set <string>::iterator wcxHandleableSetIt;

	Settings() {
		iniFileName[0] = '\0';
		setDefaults();
	}

	void setDefaults() {
		wcxmap.clear();
		fileTypeMap.clear();
		equivalent_ext.clear();

		tryCanYouHandleThisFile = false;
		listArchives = true;
		listEmptyFile = false;
		listOnlyDirectories = false;

		listColumns = 8;
		zerosInMonths = false;
		zerosInDays = false;
		zerosInHours = false;
		zerosInMinutes = false;
		zerosInSeconds = false;
		lineFormat.empty();
		getLineFormat();
	}

	void setIniLocation(HINSTANCE);

	const char* getIniFileName() {return iniFileName;}

	const bool getTryCanYouHandleThisFile() {return tryCanYouHandleThisFile;}
	const bool getListArchives() {return listArchives;}
	const bool getListEmptyFile() {return listEmptyFile;}
	const bool getListOnlyDirectories() {return listOnlyDirectories;}

	const bool getZerosInMonths() {return zerosInMonths;}
	const bool getZerosInDays() {return zerosInDays;}
	const bool getZerosInHours() {return zerosInHours;}
	const bool getZerosInMinutes() {return zerosInMinutes;}
	const bool getZerosInSeconds() {return zerosInSeconds;}

	const int getListColumns() {return listColumns;}

	const char* getLineFormat();

	void readConfig();
	bool saveConfig();

	void setTryCanYouHandleThisFile(bool val) {tryCanYouHandleThisFile = val;}
	void setListArchives(bool val) {listArchives = val;}
	void setListEmptyFile(bool val) {listEmptyFile = val;}
	void setListOnlyDirectories(bool val) {listOnlyDirectories = val;}

	void setListColumns(int val) {if (val < 1 || val > 8) val = 8; listColumns = val; lineFormat.clear();}
	void setZerosInMonths(bool val) {zerosInMonths = val; lineFormat.clear();}
	void setZerosInDays(bool val) {zerosInDays = val; lineFormat.clear();}
	void setZerosInHours(bool val) {zerosInHours = val; lineFormat.clear();}
	void setZerosInMinutes(bool val) {zerosInMinutes = val; lineFormat.clear();}
	void setZerosInSeconds(bool val) {zerosInSeconds = val; lineFormat.clear();}
};

extern Settings settings;

#endif