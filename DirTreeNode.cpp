#include "DirTreeNode.h"
#include "defs.h"

DirTreeNode* DirTreeNode::lastInserted = NULL;

DirTreeNode::DirTreeNode(const char* fName, const unsigned long long fSize, const unsigned fTime, const int fType) {
	this->name = strdup(fName);
	this->type = fType;
	this->size = fSize;
	this->time = fTime;
	this->forced = false;
	this->do_not_list_as_dir = false;
	lastInserted = (DirTreeNode *)this;
	next = NULL;
	firstChild = NULL;
}
DirTreeNode::~DirTreeNode() {
	for (DirTreeNode *p = firstChild; p != NULL; ) {
		DirTreeNode *q = p->getNext();
		delete p;
		p = q;
	}
	free(name);
}
void DirTreeNode::setNext(DirTreeNode *next) {
	this->next = next;
}
void DirTreeNode::setForced(bool bln) {
	this->forced = bln;
}
DirTreeNode* DirTreeNode::getNext() {
	return next;
}
char* DirTreeNode::getName() {
	return name;
}
// modifies firstChild, if new or replacing
bool DirTreeNode::insert(char* fName, const unsigned long long fSize, const unsigned fTime, const int fType) {
	if (fName == NULL) { // we are replacing, so free all children (if this was not created implicitly)
		if (this->forced == false) {
			for (DirTreeNode *p = firstChild; p != NULL; ) {
				DirTreeNode *q = p->getNext();
				delete p;
				p = q;
			}
			firstChild = NULL;
		}
		this->size = fSize;
		this->time = fTime;
		this->type = fType;
		this->forced = false;
		return true;
	}

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
	DirTreeNode *q = firstChild;
	for (DirTreeNode *p = firstChild; p != NULL; q = p, p = p->getNext()) {
		if (strncmp(p->getName(), fName, fNameLen) == 0 && strlen(p->getName()) == fNameLen) {
			exists = p->insert(rem, fSize, fTime, fType);
			if (exists == false) return false; // error while inserting
			exists = true;
			break;
		}
	}
	if (exists == false) { // new element
		if (rem != NULL) { // has remainder (directory which was not added before)
			char z = fName[fNameLen];
			fName[fNameLen] = '\0';
			DirTreeNode *a = new DirTreeNode(fName, 0, fTime, FILE_TYPE_DIRECTORY);
			if (q == NULL) {
				firstChild = a;
			} else {
				q->setNext(a);
			}
			fName[fNameLen] = z;
			a->insert(rem, fSize, fTime, fType);
			a->setForced(true); // created implicitly
		} else {
			DirTreeNode *a = new DirTreeNode(fName, fSize, fTime, fType);
			if (q == NULL) {
				firstChild = a;
			} else {
				q->setNext(a);
			}
		}
	}
	return true;
}
// modifies firstChild, if new or replacing
bool DirTreeNode::remove(const char* fName) {
	if (fName == NULL) { // we found it, so return true, the deletion will take place later
		return true;
	}

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
	DirTreeNode *q = firstChild;
	for (DirTreeNode *p = firstChild; p != NULL; q = p, p = p->getNext()) {
		if (strncmp(p->getName(), fName, fNameLen) == 0 && strlen(p->getName()) == fNameLen) {
			exists = p->remove(rem);
			if (exists == false) return false; // do not remove this
			exists = false;
			// we are going to remove p
			if (p == firstChild) {
				firstChild = p->getNext();
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
void DirTreeNode::writeOut(FILE* fout, char* curPath) {
	int year = (this->time >> 25) + 1980;
	int month = (this->time & 0x1FFFFFF) >> 21;
	int day = (this->time & 0x1FFFFF) >> 16;
	int hour = (this->time & 0xFFFF) >> 11;
	int minute = (this->time & 0x7FF) >> 5;
	int second = (this->time & 0x1F) * 2;

	// do not write c:\, g:\, etc. as directories
	if (this->name[1] == ':' && this->name[2] == '\\' && this->name[3] == '\0') ; // nothing
	else {
		if (this->name[strlen(this->name) - 1] == '\\') fprintf(fout, "%s", curPath);
		fprintf(fout, "%s\t%llu\t%d.%d.%d\t%d:%d.%d\n", 
			this->name,
			this->size,
			year, month, day,
			hour, minute, second);
	}

	if (firstChild != NULL) {
		int lenOrig = strlen(curPath);
		strcat(curPath, this->name);
		// first non directories
		for (DirTreeNode *p = firstChild; p != NULL; p = p->getNext()) {
			if (p->getName()[strlen(p->getName()) - 1] != '\\')
				p->writeOut(fout, curPath);
		}
		// now directories
		for (DirTreeNode *p = firstChild; p != NULL; p = p->getNext()) {
			if (p->getName()[strlen(p->getName()) - 1] == '\\')
				p->writeOut(fout, curPath);
		}
		curPath[lenOrig] = '\0';
	}
}

void DirTreeNode::finalize() {
	this->forced = false;
	for (DirTreeNode *p = firstChild; p != NULL; p = p->getNext()) {
		p->finalize();
	}
}
