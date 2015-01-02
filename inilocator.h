#ifndef INILOCATOR_H
#define INILOCATOR_H

#include <windows.h>
#include <shellapi.h>
#include <userenv.h>
#include <string>
#include "libs/inifile/IniFile.h"
using namespace std;

extern string get_wincmd_ini_location();

#endif