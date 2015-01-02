// (c) 2004 Peter Trebaticky
// Last change: 2004/02/08

#include "defs.h"
#include "DirTree.h"
#include <string.h>
#include <stdlib.h>

DirTree::DirTree() {
	firstLevel = NULL;
}
DirTree::~DirTree() {
	for (DirTreeNode *p = firstLevel; p != NULL; ) {
		DirTreeNode *q = p->getNext();
		delete p;
		p = q;
	}
}
bool DirTree::insert(const char* curDestPath, const char* fName, const unsigned long long fSize, const unsigned fTime, const int fType) {
	strcpy(fDestName, curDestPath);
	strcat(fDestName, fName);
	// rem will be pointer to remainder of path, or NULL if no remainder
	char* rem = (char*) strchr(fDestName, '\\');
	int fNameLen;
	if (rem != NULL) {
		if (rem[1] == '\0') {
			fNameLen = (int)strlen(fDestName);
			rem = NULL;
		} else {
			fNameLen = (int)(rem - fDestName) + 1;
			rem++;
		}
	} else {
		fNameLen = (int)strlen(fDestName);
	}

	bool exists = false;
	DirTreeNode *q = firstLevel;
	for (DirTreeNode *p = firstLevel; p != NULL; q = p, p = p->getNext()) {
		if (strncmp(p->getName(), fDestName, fNameLen) == 0 && strlen(p->getName()) == fNameLen) {
			exists = p->insert(rem, fSize, fTime, fType);
			if (exists == false) return false; // error while inserting
			exists = true;
			break;
		}
	}
	if (exists == false) { // new element
		if (rem != NULL) { // has remainder (directory which was not added before)
			char z = fDestName[fNameLen];
			fDestName[fNameLen] = '\0';
			DirTreeNode *a = new DirTreeNode(fDestName, 0, fTime, FILE_TYPE_DIRECTORY);
			if (q == NULL) {
				firstLevel = a;
			} else {
				q->setNext(a);
			}
			fDestName[fNameLen] = z;
			a->insert(rem, fSize, fTime, fType);
			a->setForced(true); // created implicitly
		} else {
			DirTreeNode *a = new DirTreeNode(fDestName, fSize, fTime, fType);
			if (q == NULL) {
				firstLevel = a;
			} else {
				q->setNext(a);
			}
		}
	}
	return true;
}

bool DirTree::remove(const char* fName) {
	// rem will be pointer to remainder of path, or NULL if no remainder
	char* rem = (char*) strchr(fName, '\\');
	int fNameLen;
	if (rem != NULL) {
		if (rem[1] == '\0') {
			fNameLen = (int)strlen(fName);
			rem = NULL;
		} else {
			fNameLen = (int)(rem - fName) + 1;
			rem++;
		}
	} else {
		fNameLen = (int)strlen(fName);
	}

	bool exists = false;
	DirTreeNode *q = firstLevel;
	for (DirTreeNode *p = firstLevel; p != NULL; q = p, p = p->getNext()) {
		if (strncmp(p->getName(), fName, fNameLen) == 0 && strlen(p->getName()) == fNameLen) {
			exists = p->remove(rem);
			if (exists == false) return true; // do not remove this
			exists = true;
			// we are going to remove p
			if (p == firstLevel) {
				firstLevel = p->getNext();
			} else {
				q->setNext(p->getNext());
			}
			delete p;
			break;
		}
	}
	if (exists == false) return false;
	return true;
}

void DirTree::writeOut(FILE* fout, char* curPath) {
	// first non directories
	for (DirTreeNode *p = firstLevel; p != NULL; p = p->getNext()) {
		if (p->do_not_list_as_dir || p->getName()[strlen(p->getName()) - 1] != '\\') {
			if (p->getName()[strlen(p->getName()) - 1] == '\\') p->getName()[strlen(p->getName()) - 1] = '\0';
			p->writeOut(fout, curPath);
		}
	}
	// now directories
	for (DirTreeNode *p = firstLevel; p != NULL; p = p->getNext()) {
		if (!p->do_not_list_as_dir && p->getName()[strlen(p->getName()) - 1] == '\\')
			p->writeOut(fout, curPath);
	}
}

void DirTree::finalize() {
	for (DirTreeNode *p = firstLevel; p != NULL; p = p->getNext()) {
		p->finalize();
	}
}
