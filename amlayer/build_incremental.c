/* build_incremental.c
 * Build index by incremental inserts into an empty index.
 *
 * Usage:
 *   ./build_incremental [sp_file] [indexNo] [roll_field_index]
 * Defaults:
 *   sp_file = sp_student.dat
 *   indexNo = 2
 *   fieldIndex = 1
 *
 * Output: am_build_incremental.csv
 */

#include "am.h"
#include "../pflayer/splayer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define DEFAULT_SP "sp_student.dat"
#define OUTCSV "am_build_incremental.csv"

static int extract_key_from_record(const char *rec, int field_index) {
    char *tmp = strdup(rec);
    char *p = tmp;
    int idx = 0;
    char *tok;
    int key = 0;
    while ((tok = strsep(&p, ";")) != NULL) {
        if (idx == field_index) { key = atoi(tok); break; }
        idx++;
    }
    free(tmp);
    return key;
}

int main(int argc, char **argv) {
    const char *spfile = (argc > 1) ? argv[1] : DEFAULT_SP;
    int indexNo = (argc > 2) ? atoi(argv[2]) : 2;
    int fieldIndex = (argc > 3) ? atoi(argv[3]) : 1;

    printf("=== Build index incremental: %s (indexNo=%d) ===\n", spfile, indexNo);

    int spfd = SP_OpenFile(spfile);
    if (spfd < 0) { perror("SP_OpenFile"); return 1; }

    /* create empty index */
    if (AM_CreateIndex("student", indexNo, 'i', 4) != AME_OK) {
        AM_PrintError("AM_CreateIndex");
    }

    /* measure */
    clock_t tstart = clock();
    unsigned long beforeLogical = PFbufferPool.logicalPageRequests;
    unsigned long beforePhysReads = PFbufferPool.physicalReads;
    unsigned long beforePhysWrites = PFbufferPool.physicalWrites;

    SP_Scan scan;
    SP_ScanInit(&scan, spfd);
    char *rec; int rlen; SP_RecId rid;
    long inserted = 0;
    char valbuf[4];

    while (SP_ScanNext(&scan, &rec, &rlen, &rid) == 0) {
        int key = extract_key_from_record(rec, fieldIndex);
        memcpy(valbuf, &key, 4);

        /* open index PF file and insert (AM_InsertEntry signature varies by impl)
           typical: AM_InsertEntry(fileDesc, attrType, attrLength, value, recId)
        */
        int amFd = PF_OpenFile("student.2");
        if (amFd < 0) {
            if (AM_InsertEntry(0, 'i', 4, valbuf, (int)rid) != AME_OK) AM_PrintError("AM_InsertEntry");
        } else {
            if (AM_InsertEntry(amFd, 'i', 4, valbuf, (int)rid) != AME_OK) AM_PrintError("AM_InsertEntry");
            PF_CloseFile(amFd);
        }

        free(rec); inserted++;
        if (inserted % 1000 == 0) { printf("."); fflush(stdout); }
    }
    SP_ScanClose(&scan);

    clock_t tend = clock();
    double seconds = (double)(tend - tstart) / CLOCKS_PER_SEC;
    unsigned long logicalDiff = PFbufferPool.logicalPageRequests - beforeLogical;
    unsigned long physReadsDiff = PFbufferPool.physicalReads - beforePhysReads;
    unsigned long physWritesDiff = PFbufferPool.physicalWrites - beforePhysWrites;

    printf("\nInserted %ld entries in %.2f sec\n", inserted, seconds);
    printf("LogicalPageRequests=%lu physicalReads=%lu physicalWrites=%lu\n",
           logicalDiff, physReadsDiff, physWritesDiff);

    FILE *csv = fopen(OUTCSV, "w");
    if (csv) {
        fprintf(csv, "method,records,time_sec,logicalReq,physReads,physWrites\n");
        fprintf(csv, "build_incremental,%ld,%.4f,%lu,%lu,%lu\n",
                inserted, seconds, logicalDiff, physReadsDiff, physWritesDiff);
        fclose(csv);
    }
    SP_CloseFile(spfd);
    printf("Results written to %s\n", OUTCSV);
    return 0;
}
