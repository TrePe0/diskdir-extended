// (c) 2006 Peter Trebaticky
// Last change: 2006/12/24

#ifndef DEFS_H
#define DEFS_H

#include <map>
#include <string>

enum FILE_TYPE_ENUM {
	FILE_TYPE_REGULAR,
	FILE_TYPE_DIRECTORY,
	FILE_TYPE_ACE,
	FILE_TYPE_ARJ,
	FILE_TYPE_CAB,
	FILE_TYPE_JAR,
	FILE_TYPE_RAR,
	FILE_TYPE_TAR,
	FILE_TYPE_TBZ,
	FILE_TYPE_TGZ,
	FILE_TYPE_ZIP,
	FILE_TYPE_ISO,
	FILE_TYPE_BY_WCX,
	FILE_TYPE_LAST
};

enum LIST_OPTION_ENUM {
	LIST_YES,
	LIST_NO,
	LIST_ASK
};

struct FILE_TYPE_ELEM {
	FILE_TYPE_ENUM fileType;
	std::map <std::string, std::pair<std::string, char> >::iterator which_wcx;
	LIST_OPTION_ENUM list_this;
};

#endif
