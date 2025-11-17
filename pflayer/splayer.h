#ifndef SPLAYER_H
#define SPLAYER_H

#include "pf.h"
#include <stdint.h>

typedef unsigned int SP_RecId; /* encoded: (pageNum << 16) | slotIndex */

#define SP_MAGIC 0x53504C54 /* "SPLT" */

int SP_CreateFile(const char *fileName);
int SP_DestroyFile(const char *fileName);
int SP_OpenFile(const char *fileName);
int SP_CloseFile(int fd);

/* Insert record data (len bytes). Returns AME_OK (0) on success and sets *recId */
int SP_InsertRecord(int fd, const char *data, int len, SP_RecId *recId);

/* Delete record by recId */
int SP_DeleteRecord(int fd, SP_RecId recId);

/* Get record content: user buffer should be large enough; returns length via *len */
int SP_GetRecord(int fd, SP_RecId recId, char *buf, int *len);

/* Scanner (simple): returns 0 on success, -1 on EOF */
typedef struct {
    int fd;
    int curPageNum;
    char *pageBuf;
    int slotIndex;
    int initialized;
} SP_Scan;

int SP_ScanInit(SP_Scan *scan, int fd);
int SP_ScanNext(SP_Scan *scan, char **outBuf, int *outLen, SP_RecId *outRecId);
int SP_ScanClose(SP_Scan *scan);

/* Utility: compute per-page utilization for given fd (returns utilization as fraction *100) */
double SP_ComputeSpaceUtilization(int fd, int *out_pages, long *out_total_bytes);

/* Compact page list: compacts a specific page (pageNum) */
int SP_CompactPage(int fd, int pageNum);

#endif /* SPLAYER_H */
