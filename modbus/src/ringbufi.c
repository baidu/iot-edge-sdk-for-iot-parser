
#include "ringbufi.h"
#include <stdlib.h>
void saveMeta(RingBuFi* pBuf);
size_t metaLen();

size_t readSizet(FILE* fp, size_t offset) ;

size_t writeBytesAt(FILE* fp, size_t offset, const void* pBytes, size_t len) ;


RingBuFi* newRingBuFi(const char* file, size_t sizeLimit) {

	if (file == NULL) {
		return NULL;
	}
	// check existence of the file
	RingBuFi* pBuf = malloc(sizeof(RingBuFi));
	pBuf->sizeLimit = sizeLimit;
	FILE* fp = fopen(file, "r+b");
	if (fp == NULL) {
		// file not exist, create it
		fp = fopen(file, "w+b");
		if (fp == NULL) {
			// failed to create the file
			return NULL;
		}

		pBuf->fp = fp;
		pBuf->sizeLimit = sizeLimit;
		pBuf->next = metaLen();
		pBuf->head = pBuf->next;
		pBuf->nomanland = pBuf->next;
		pBuf->blockCnt = 0;
		// init the file
		saveMeta(pBuf);
	} else {
		fread((void*)&pBuf->head, sizeof(pBuf->head), 1, fp);
		fread((void*)&pBuf->next, sizeof(pBuf->next), 1, fp);
		fread((void*)&pBuf->nomanland, sizeof(pBuf->nomanland), 1, fp);
		fread((void*)&pBuf->blockCnt, sizeof(pBuf->blockCnt), 1, fp);
		fread((void*)&pBuf->sizeLimit, sizeof(pBuf->sizeLimit), 1, fp);
		pBuf->fp = fp;
	}
	return pBuf;
}

void saveMeta(RingBuFi* pBuf) {
	if (pBuf != NULL && pBuf->fp != NULL) {
		fseek(pBuf->fp, 0, SEEK_SET);
		fwrite((const void*)&pBuf->head, sizeof(pBuf->head), 1, pBuf->fp);
		fwrite((const void*)&pBuf->next, sizeof(pBuf->next), 1, pBuf->fp);
		fwrite((const void*)&pBuf->nomanland, sizeof(pBuf->nomanland), 1, pBuf->fp);
		fwrite((const void*)&pBuf->blockCnt, sizeof(pBuf->blockCnt), 1, pBuf->fp);
		fwrite((const void*)&pBuf->sizeLimit, sizeof(pBuf->sizeLimit), 1, pBuf->fp);
		fflush(pBuf->fp);
	}
}

int isRingBuFiEmpty(const RingBuFi *pBuf) 
{
	return (pBuf == NULL || pBuf->blockCnt <= 0) ? 1 : 0;
}

void increaseBlockCnt(RingBuFi *pBuf, int cnt)
{
	pBuf->blockCnt += cnt;
}

size_t eraseEnoughSpaceOrEof(RingBuFi* pBuf, size_t bytesFreed, size_t byteToFree)
{
	size_t bytesFreedTotal = bytesFreed;
	while (bytesFreedTotal < byteToFree && pBuf->head < pBuf->nomanland) {
		size_t recordLen = readSizet(pBuf->fp, pBuf->head);
		bytesFreedTotal += recordLen + sizeof(size_t);
		pBuf->head += recordLen + sizeof(size_t);
		increaseBlockCnt(pBuf, -1); // pBuf->blockCnt--;
	}

	return bytesFreedTotal;
}

int putRingBuFiRecord(RingBuFi* pBuf, const void* pBytes, size_t len) {
	if (pBuf == NULL || pBuf->fp == NULL || pBytes == NULL || len <= 0) {
		return 0;
	}
	if (pBuf->sizeLimit - metaLen() < len + sizeof(len))
	{
		return 0; 	// limit is too small
	}

	size_t sizeToWrite = sizeof(size_t) + len;
	if (pBuf->blockCnt <= 0)
	{
		// the buf is empty, start from the beginning
		pBuf->next = metaLen();
		pBuf->head = pBuf->next;
		writeBytesAt(pBuf->fp, pBuf->next, (const void*) &len, sizeof(len));
		writeBytesAt(pBuf->fp, pBuf->next + sizeof(len), pBytes, len);
		pBuf->blockCnt = 1;
		pBuf->nomanland = pBuf->next + sizeToWrite;
		pBuf->next += sizeToWrite;
		saveMeta(pBuf);
		// truncate the file to minimize the disk usage
		if (ftruncate(fileno(pBuf->fp), pBuf->nomanland) != 0)
		{
			printf("[WARN] failed to shorten the file size by ftruncate, no functional impact\r\n");
		}
	}
	else if (pBuf->next > pBuf->head) {
		if (pBuf->next + sizeToWrite > pBuf->sizeLimit) {
			// exceeded the sizeLimit, write to the start of data block
			// override record(s) at the beginning of data block if needed 
			size_t bytesFreed = pBuf->head - metaLen();
			bytesFreed = eraseEnoughSpaceOrEof(pBuf, bytesFreed, sizeToWrite);

			writeBytesAt(pBuf->fp, metaLen(), (const void*) &len, sizeof(len));
			writeBytesAt(pBuf->fp, metaLen() + sizeof(size_t), pBytes, len);
			increaseBlockCnt(pBuf, 1); // pBuf->blockCnt++;
			pBuf->nomanland = pBuf->next;
			if (pBuf->head >= pBuf->nomanland)
			{
				pBuf->head = metaLen();
			}
			pBuf->next = sizeToWrite + metaLen();
			saveMeta(pBuf);
		} else {
			// just put the block to the end of file
			size_t newNext = pBuf->next + sizeToWrite; 
			writeBytesAt(pBuf->fp, pBuf->next, (const void*) &len, sizeof(len)); 
			size_t byteWritten = writeBytesAt(pBuf->fp, pBuf->next + sizeof(len), pBytes, len);
			increaseBlockCnt(pBuf, 1); // pBuf->blockCnt++;
			pBuf->next = newNext;
			pBuf->nomanland = pBuf->next;
			saveMeta(pBuf);
		}
			
	} else { // if (pBuf->next <= pBuf->head) {

		if (pBuf->head - pBuf->next >= sizeToWrite) {
			// there is enough space between head and next
		} else {
			if (pBuf->nomanland - pBuf->next >= sizeToWrite)
			{
				// erase existing records should be enough
				size_t bytesFreed = pBuf->head - pBuf->next;
				bytesFreed = eraseEnoughSpaceOrEof(pBuf, bytesFreed, sizeToWrite);
				if (pBuf->head >= pBuf->nomanland)
				{
					pBuf->head = metaLen();
				}
			} 
			else if (pBuf->sizeLimit - pBuf->next >= sizeToWrite)
			{
				// all the records after head will be erased,
				// erase all the records after head
				eraseEnoughSpaceOrEof(pBuf, 0, pBuf->sizeLimit);
				pBuf->head = metaLen();
				pBuf->nomanland = pBuf->next;
			} 
			else
			{
				// erase from the begining
				if (pBuf->next - metaLen() >= sizeToWrite)
				{
					// erase from the beinning
					pBuf->nomanland = pBuf->next;
					pBuf->head = metaLen();

					size_t bytesFreed = eraseEnoughSpaceOrEof(pBuf, 0, sizeToWrite);
				} 
				else
				{
					// every thing will be erased
					pBuf->next = metaLen();
					pBuf->head = pBuf->next;
					pBuf->nomanland = pBuf->next;
					pBuf->blockCnt = 0;
				}
			}
		}
		writeBytesAt(pBuf->fp, pBuf->next, (const void*) &len, sizeof(len));
		writeBytesAt(pBuf->fp, pBuf->next + sizeof(size_t), pBytes, len);
		increaseBlockCnt(pBuf, 1);
		pBuf->next += sizeToWrite;
		if (pBuf->next > pBuf->head)
		{
			pBuf->nomanland = pBuf->next;
		}
		saveMeta(pBuf);
	}
	
	return len;
}

int peekRingBuFiRecord(const RingBuFi* pBuf, void** ppBytes, size_t* pLen) {
	if (pBuf == NULL || pBuf->fp == NULL || pBuf->head == pBuf->next || pLen == NULL) {
		return 0;
	}

	*pLen = readSizet(pBuf->fp, pBuf->head);
	*ppBytes = malloc(*pLen);
	
	fseek(pBuf->fp, pBuf->head + sizeof(size_t), SEEK_SET);	// we may save this line
	fread(*ppBytes, *pLen, 1, pBuf->fp);

	return 1;
}

int popRingBuFiRecord(RingBuFi* pBuf) {
	if (pBuf == NULL || pBuf->fp == NULL || pBuf->blockCnt == 0) {
		return 0;
	}

	int len = readSizet(pBuf->fp, pBuf->head);

	pBuf->head = pBuf->head + len + sizeof(size_t);
	if (pBuf->head > pBuf->next && pBuf->head >= pBuf->nomanland) 
	{
		pBuf->head = metaLen();
	}
	pBuf->blockCnt--;
	if (pBuf->blockCnt <= 0)
	{
		pBuf->next = metaLen();
		pBuf->head = pBuf->next;
		pBuf->nomanland = pBuf->next;
		pBuf->blockCnt = 0;
		// truncate the file to minimize the disk usage
		if (ftruncate(fileno(pBuf->fp), metaLen()) != 0)
		{
			printf("[WARN] failed to shorten the file size by ftruncate, no functional impact\r\n");
		}
	}
	saveMeta(pBuf);
}

void closeRingBuFi(RingBuFi* pBuf) {
	if (pBuf != NULL && pBuf->fp != NULL) {
		fclose(pBuf->fp);
	}
}

size_t metaLen() {
	return sizeof(size_t) * 5;
}

size_t readSizet(FILE* fp, size_t offset) {
	int seekrc = fseek(fp, offset, SEEK_SET);
	size_t val = 0;
	size_t rc = fread((void*) &val, sizeof(val), 1, fp);
	return val;
}

size_t writeBytesAt(FILE* fp, size_t offset, const void* pBytes, size_t len) {
	fseek(fp, offset, SEEK_SET);
	size_t ret = fwrite(pBytes, len, 1, fp);
	fflush(fp);
	return ret;
}


