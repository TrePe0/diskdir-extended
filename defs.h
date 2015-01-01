// (c) 2004 Peter Trebaticky
// Last change: 2004/02/18

#ifndef DEFS_H
#define DEFS_H

enum 
{
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
	FILE_TYPE_LAST
};

struct ListItem
{
	char* name;
	int type;
	unsigned size;
	unsigned time;
	unsigned char depth; // number of '\\' in name
	int prev, next;
};

#endif
