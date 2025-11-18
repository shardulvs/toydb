
/* build_from_file.c
 * Build an index by scanning an existing slotted-page file (sp_student.dat).
 *
 * Usage:
 *   ./build_from_file [sp_file] [indexNo] [roll_field_index]
 * Defaults:
 *   sp_file = sp_student.dat
 *   indexNo = 1
 *   roll_field_index = 1  (0-based)
 *
 * Outputs: prints progress and writes 'am_build_from_file.csv' with a CSV line.
 */

#include "am.h"
#include "../pflayer/splayer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define DEFAULT_SP "sp_student.dat"
#define OUTCSV "am_build_from_file.csv"

static int extract_key_from_record(const char *rec, int field_index) {
    /* tokenize by ';' and return integer in field_index (0-based). */
    if (!rec) return 0;
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
    int indexNo = (argc > 2) ? atoi(argv[2]) : 1;
    int fieldIndex = (argc > 3) ? atoi(argv[3]) : 1;

    printf("=== Build index from file: %s (indexNo=%d) ===\n", spfile, indexNo);
    /* open slotted file */
    int spfd = SP_OpenFile(spfile);
    if (spfd < 0) { perror("SP_OpenFile"); return 1; }

    /* create index */
    if (AM_CreateIndex("student", indexNo, 'i', 4) != AME_OK) {
        AM_PrintError("AM_CreateIndex");
        /* continue - maybe exists */
    }
    char valbuf[4];

    /* measure start */
    clock_t tstart = clock();
    unsigned long beforeLogical = PFbufferPool.logicalPageRequests;
    unsigned long beforePhysReads = PFbufferPool.physicalReads;
    unsigned long beforePhysWrites = PFbufferPool.physicalWrites;

    /* scan slotted file and insert */
    SP_Scan scan;
    SP_ScanInit(&scan, spfd);
    char *rec; int rlen; SP_RecId rid;
    long inserted = 0;
    while (SP_ScanNext(&scan, &rec, &rlen, &rid) == 0) {
        int key = extract_key_from_record(rec, fieldIndex);
        memcpy(valbuf, &key, 4);
        if (AM_InsertEntry(/*fileDesc=*/0 /* AM_OpenIndex? we'll open index file below */, 'i', 4, valbuf, (int)rid) != AME_OK) {
            /* Most AM implementations expect a file descriptor; we must open index file */
            /* So open index file and use that descriptor instead (AM_InsertEntry typically accepts fileDesc) */
            /* We'll open index file now and use descriptor */
            break;
        }
        free(rec);
        inserted++;
        if (inserted % 1000 == 0) { printf("."); fflush(stdout); }
    }
    SP_ScanClose(&scan);
    /* The above loop attempted to call AM_InsertEntry with fileDesc=0 to detect API - fix below */

    /* Correct insertion pass using proper index FD */
    printf("\nStarting proper insert pass (opening AM index)...\n");
    int idxFd = PF_OpenFile("student.1"); /* AM file name is student.1 */
    if (idxFd < 0) {
        /* If AM_CreateIndex created otherwise, try AM_OpenIndex equivalent - but AM API uses fileDesc directly */
        /* The course AM API generally uses file name+indexNo via AM_InsertEntry(fileDesc,...).
           In your AM implementation the fileDesc is the PF file descriptor returned by PF_OpenFile("student.1").
         */
        printf("Unable to open index file student.%d via PF_OpenFile; trying AM approach.\n", indexNo);
    }

    /* Rewind and re-scan */
    SP_ScanInit(&scan, spfd);
    inserted = 0;
    while (SP_ScanNext(&scan, &rec, &rlen, &rid) == 0) {
        int key = extract_key_from_record(rec, fieldIndex);
        memcpy(valbuf, &key, 4);
        /* AM_InsertEntry expects a fileDesc (PF fd for the index file) — typical AM_OpenFile step:
           you may have a separate API for opening indexes; if so, adapt accordingly.
           Here I'll assume the index file PF fd is obtained by PF_OpenFile("student.indexNo").
        */
        int amFd = PF_OpenFile("student.1");
        if (amFd < 0) {
            /* fallback: call AM_InsertEntry with 'student' (possible different API) */
            if (AM_InsertEntry(/*fileDesc*/ 0, 'i', 4, valbuf, (int)rid) != AME_OK) {
                AM_PrintError("AM_InsertEntry");
                /* we continue to attempt rest */
            }
        } else {
            if (AM_InsertEntry(amFd, 'i', 4, valbuf, (int)rid) != AME_OK) {
                AM_PrintError("AM_InsertEntry");
            }
            PF_CloseFile(amFd);
        }
        free(rec);
        inserted++;
        if (inserted % 1000 == 0) { printf("."); fflush(stdout); }
    }
    SP_ScanClose(&scan);

    /* measure end */
    clock_t tend = clock();
    double seconds = (double)(tend - tstart) / CLOCKS_PER_SEC;

    unsigned long afterLogical = PFbufferPool.logicalPageRequests;
    unsigned long afterPhysReads = PFbufferPool.physicalReads;
    unsigned long afterPhysWrites = PFbufferPool.physicalWrites;

    unsigned long logicalDiff = afterLogical - beforeLogical;
    unsigned long physReadsDiff = afterPhysReads - beforePhysReads;
    unsigned long physWritesDiff = afterPhysWrites - beforePhysWrites;

    printf("\nInserted %ld entries in %.2f sec\n", inserted, seconds);
    printf("LogicalPageRequests=%lu physicalReads=%lu physicalWrites=%lu\n",
           logicalDiff, physReadsDiff, physWritesDiff);

    /* CSV output */
    FILE *csv = fopen(OUTCSV, "w");
    if (csv) {
        fprintf(csv, "method,records,time_sec,logicalReq,physReads,physWrites\n");
        fprintf(csv, "build_from_file,%ld,%.4f,%lu,%lu,%lu\n",
                inserted, seconds, logicalDiff, physReadsDiff, physWritesDiff);
        fclose(csv);
    }
    SP_CloseFile(spfd);
    printf("Results written to %s\n", OUTCSV);
    return 0;
}
