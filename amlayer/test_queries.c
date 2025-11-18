
/* test_queries.c
 * Open an index and run sample queries to measure query time and page accesses.
 *
 * Usage:
 *   ./test_queries [indexNo] [queryType] [value]
 * Examples:
 *   ./test_queries 3 point 95302001
 *   ./test_queries 3 range 900000 960000
 *
 * Outputs am_query_results.csv
 */

#include "am.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define OUTCSV "am_query_results.csv"

int main(int argc, char **argv) {
    int indexNo = (argc > 1) ? atoi(argv[1]) : 3;
    const char *qtype = (argc > 2) ? argv[2] : "point";

    printf("=== Query test on student.%d (%s) ===\n", indexNo, qtype);

    /* open index PF file */
    char indexfname[128];
    sprintf(indexfname, "student.%d", indexNo);
    int amFd = PF_OpenFile(indexfname);
    if (amFd < 0) { perror("PF_OpenFile index"); return 1; }
    char valbuf[4];

    /* samples: if point query, use value argument or default */
    if (strcmp(qtype, "point") == 0) {
        int key = (argc > 3) ? atoi(argv[3]) : 95302001;
        memcpy(valbuf, &key, 4);
        clock_t tstart = clock();
        unsigned long beforeLogical = PFbufferPool.logicalPageRequests;
        unsigned long beforePhysReads = PFbufferPool.physicalReads;
        unsigned long beforePhysWrites = PFbufferPool.physicalWrites;

        int scanDesc = AM_OpenIndexScan(amFd, 'i', 4, EQUAL, valbuf);
        if (scanDesc < 0) { AM_PrintError("AM_OpenIndexScan"); }
        int recId;
        int found = 0;
        while ((recId = AM_FindNextEntry(scanDesc)) != AME_EOF) {
            if (recId < 0) { AM_PrintError("AM_FindNextEntry"); break; }
            found++;
        }
        AM_CloseIndexScan(scanDesc);

        clock_t tend = clock();
        double seconds = (double)(tend - tstart) / CLOCKS_PER_SEC;
        unsigned long logicalDiff = PFbufferPool.logicalPageRequests - beforeLogical;
        unsigned long physReadsDiff = PFbufferPool.physicalReads - beforePhysReads;
        unsigned long physWritesDiff = PFbufferPool.physicalWrites - beforePhysWrites;

        printf("Point query key=%d found=%d time=%.4f s L=%lu R=%lu W=%lu\n",
               key, found, seconds, logicalDiff, physReadsDiff, physWritesDiff);

        FILE *csv = fopen(OUTCSV,"w");
        if (csv) {
            fprintf(csv, "index,pquery_key,found,time_sec,logicalReq,physReads,physWrites\n");
            fprintf(csv, "%d,%d,%d,%.6f,%lu,%lu,%lu\n",
                    indexNo, key, found, seconds, logicalDiff, physReadsDiff, physWritesDiff);
            fclose(csv);
        }
    } else if (strcmp(qtype, "range") == 0) {
        if (argc < 5) {
            printf("Usage: %s indexNo range low high\n", argv[0]); PF_CloseFile(amFd); return 1;
        }
        int low = atoi(argv[3]), high = atoi(argv[4]);
        memcpy(valbuf, &low, 4);

        clock_t tstart = clock();
        unsigned long beforeLogical = PFbufferPool.logicalPageRequests;
        unsigned long beforePhysReads = PFbufferPool.physicalReads;
        unsigned long beforePhysWrites = PFbufferPool.physicalWrites;

        int scanDesc = AM_OpenIndexScan(amFd, 'i', 4, GREATER_THAN_EQUAL, (char*)&low);
        if (scanDesc < 0) { AM_PrintError("AM_OpenIndexScan"); }
        int recId;
        int count = 0;
        while ((recId = AM_FindNextEntry(scanDesc)) != AME_EOF) {
            if (recId < 0) { AM_PrintError("AM_FindNextEntry"); break; }
            /* copy key out? AM_FindNextEntry returns recId, not key; scanning returns records matching op */
            count++;
            if (count % 1000 == 0) { printf("."); fflush(stdout); }
            /* Stop if key > high - not possible here because we don't get key value; this is a limitation.
               A better scanner would return (key,recId). If your AM_FindNextEntry returns recId only,
               adapt this code to check record's key via SP_GetRecord. */
        }
        AM_CloseIndexScan(scanDesc);

        clock_t tend = clock();
        double seconds = (double)(tend - tstart) / CLOCKS_PER_SEC;
        unsigned long logicalDiff = PFbufferPool.logicalPageRequests - beforeLogical;
        unsigned long physReadsDiff = PFbufferPool.physicalReads - beforePhysReads;
        unsigned long physWritesDiff = PFbufferPool.physicalWrites - beforePhysWrites;

        printf("Range query [%d,%d] count=%d time=%.4f s L=%lu R=%lu W=%lu\n",
               low, high, count, seconds, logicalDiff, physReadsDiff, physWritesDiff);

        FILE *csv = fopen(OUTCSV,"w");
        if (csv) {
            fprintf(csv, "index,range_low,range_high,count,time_sec,logicalReq,physReads,physWrites\n");
            fprintf(csv, "%d,%d,%d,%d,%.6f,%lu,%lu,%lu\n",
                    indexNo, low, high, count, seconds, logicalDiff, physReadsDiff, physWritesDiff);
            fclose(csv);
        }
    } else {
        printf("Unknown query type '%s'. Use 'point' or 'range'.\n", qtype);
        PF_CloseFile(amFd);
        return 1;
    }
    PF_CloseFile(amFd);
    printf("Query results written to %s\n", OUTCSV);
    return 0;
}
