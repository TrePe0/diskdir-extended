// (c) 2004 Peter Trebaticky
// Last change: 2004/02/08

#include "defs.h"
#include "SortedList.h"
#include <string.h>
#include <stdlib.h>

SortedList::SortedList()
{
	list = NULL;
	size = num = last = first = 0;
}

SortedList::~SortedList()
{
	Destroy();
}

bool SortedList::Create()
{
	if(list != NULL) return false; // list already exists

	num = last = first = 0;
	size = 1000;
	list = (ListItem*) malloc(size * sizeof(ListItem));
	if(list == NULL)
	{
		size = 0;
		return false;
	}
	return true;
}

void SortedList::Destroy()
{
	for(int i = 0; i < num; i++) free(list[i].name);
	free(list);
	list = NULL;
	size = num = last = first = 0;
}

bool SortedList::Insert(const char* fName, const unsigned fSize, const unsigned fTime, const int fType)
{
	if(num == size) // we have to expand the list
	{
		ListItem* newList = (ListItem*) realloc(list, (size + 1000) * sizeof(ListItem));
		if(newList == NULL) return false;
		size += 1000;
		list = newList;
	}
	if((list[num].name = (char*) malloc(strlen(fName) + 1)) == NULL) return false;
	strcpy(list[num].name, fName);
	list[num].size = fSize;
	list[num].time = fTime;
	list[num].type = fType;
	int i;
	for(i = list[num].depth = 0; fName[i] != '\0'; i++)
		if(fName[i] == '\\') list[num].depth++;

	// insert this item in appropriate place using 'ListItem.prev' and variable 'last'
	if(num == 0) 
	{
		list[num].prev = -1;
		list[num].next = -1;
		last = num;
		first = num;
	}
	else
	{
		int aktnum = last;
		int aktdepth = list[last].depth;
		int max = last;
		int maxd = 0, maxdprev, h;
		
		i = 0;
		while(list[num].name[i] == list[aktnum].name[i] && list[num].name[i] != '\0')
		{
			if(list[num].name[i] == '\\') maxd = i;
			i++;
		}

		while(aktnum != -1)
		{
			i = 0; maxdprev = maxd; maxd = 0; h = 0;
			while(list[num].name[i] == list[aktnum].name[i] && list[num].name[i] != '\0')
			{
				if(list[num].name[i] == '\\') {h++; maxd = i;}
				i++;
			}
			if(maxd >= maxdprev)
			{
				if(maxd > maxdprev)
				{
					max = aktnum;
				}
				else
				{
					if(list[num].depth == h)
					{
						if(list[aktnum].depth > h || list[aktnum].type > list[num].type)
							max = aktnum;
						else break;
					}
					else if(list[aktnum].depth == h)
					{
						if(list[aktnum].type == 0) break;
					}
				}
			}
			else break;

			aktnum = list[aktnum].prev;
		}

		i = 0; h = 0;
		while(list[num].name[i] == list[max].name[i] && list[num].name[i] != '\0')
		{
			if(list[num].name[i] == '\\') h++;
			i++;
		}

		if((list[num].depth == h && (list[max].depth > h || list[max].type < list[num].type)))
		{
			// (...,) num, max, ...
			if(max == first) first = num;
			else list[list[max].prev].next = num;
			list[num].prev = list[max].prev;
			list[max].prev = num;
			list[num].next = max;
		}
		else
		{
			// max, num(, ...)
			if(max == last) last = num;
			else list[list[max].next].prev = num;
			list[num].next = list[max].next;
			list[num].prev = max;
			list[max].next = num;
		}
	}

	num++;
	return true;
}
