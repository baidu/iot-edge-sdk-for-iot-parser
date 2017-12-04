/*
 This implement a ring buffer, backed by filesystem.
 each record is a byte array with variable length.
 The max size of the cache file is configurable.
 Two files will be used, one for content, the other
 one for metadata.
*/

#ifndef _FILE_BASED_RING_BUFFER_H_
#define _FILE_BASED_RING_BUFFER_H_
#include <stdio.h>
typedef struct {
	char* file;	// the file path
	FILE* fp;	// the file handler
	size_t blockCnt;	// number of records
	size_t sizeLimit;	// max file size can be used
	size_t next;	// file offset for the next record
	size_t head;		// file offset of the first (eldest) record
	size_t nomanland;	// stop reading after this offset
} RingBuFi;


// create a new buffer if the file does not exist, otherwise, initialize from
// the existing file
RingBuFi* newRingBuFi(const char* file, size_t sizeLimit);

// return 1 if it is empty
int isRingBuFiEmpty(const RingBuFi* pBuf);

// put, return the number of byte cached
int putRingBuFiRecord(RingBuFi* pBuf, const void* pBytes, size_t len);

// peek, return 0 on success, otherwise -1. the data is returned 
// in ppBytes, the record lenth in pLen, the caller is resposibile
// to free ppBytes
int peekRingBuFiRecord(const RingBuFi* pBuf, void** ppBytes, size_t* pLen);

// pop, skip the first record if any
// return 0 on success, otherwise -1
int popRingBuFiRecord(RingBuFi* pBuf);

void closeRingBuFi(RingBuFi* pBuf);
#endif


