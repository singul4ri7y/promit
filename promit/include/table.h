#ifndef _promit_table_
#define _promit_table_

#pragma once

#include "value.h"

typedef struct {
	ObjString* key;
	ValueContainer valueContainer;
} Entry;

typedef struct {
	int count;
	int capacity;
	int tombstones;
	Entry* entries;
} Table;

void initTable(Table*);
Entry* findEntry(Entry*, size_t, ObjString*);
bool tableInsert(Table*, ObjString*, ValueContainer);
bool tableGet(Table*, ObjString*, ValueContainer*);
void tableInsertAll(Table*, Table*);
bool tableDelete(Table*, ObjString*);
ObjString* tableFindString(Table*, const char*, int, uint32_t);
void markTable(Table*);
void tableRemoveWhite(Table*);
void freeTable(Table*);

#endif
