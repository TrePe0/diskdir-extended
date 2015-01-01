// (c) 2004 Peter Trebaticky
// Last change: 2004/02/07

#ifndef SORTED_LIST_H
#define SORTED_LIST_H

class SortedList
{
private:
	int size; // number of allocated list items
	int num;  // number of valid items
	int first, last; // item
	ListItem* list;
public:
	int GetNum() {return num;}
	int GetFirst() {return first;}
	ListItem* GetList() const {return list;}
	bool Create();
	void Destroy();
	void Clear() {num = 0;}
	bool Insert(const char* fName, const unsigned fSize, const unsigned fTime, const int fType);
	SortedList();
	~SortedList();
};

#endif
