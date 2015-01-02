// (c) 2004 Peter Trebaticky
// Last change: 2004/02/07
// 2005/12/26

#ifndef DIR_TREE_H
#define DIR_TREE_H

#include "DirTreeNode.h"
#include "wcxhead.h"

class DirTree {
private:
	char fDestName[MAX_FULL_PATH_LEN]; // whole file name (curDestPath + fName)
	DirTreeNode *firstLevel; // points to first element on first level, or NULL
public:
	DirTree();
	~DirTree();
	bool insert(const char* curDestPath, const char* fName, const unsigned long long fSize, const unsigned fTime, const int fType);
	bool remove(const char* fName);
	void writeOut(FILE* fout, char* curPath);
	void finalize(); // makes all nodes explicit, i.e. forced = false
	void doNotListAsDirLastInserted() {
		DirTreeNode::setDoNotListAsDirForLastInserted();
	}
};

#endif
