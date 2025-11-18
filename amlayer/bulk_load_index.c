
/* bulk_load_index.c
 * Bulk-load index by sorting all (key,recId) pairs and inserting in sorted order.
 * This approximates a bottom-up bulk load by inserting sorted keys (minimizes split churn).
 *
 * Usage:
 *   ./bulk_load_index [sp_file] [indexNo] [roll_field_index]
 * Defaults:
 *   sp_file = sp_student.dat
 *   indexNo = 3
 *   roll_field_index = 1
 *
 * Output: am_bulk_load.csv
 */

#include "am.h"
#include "../pflayer/splayer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define DEFAULT_SP "sp_student.dat"
#define OUTCSV "am_bulk_load.csv"

typedef struct {
    int key;
    int recId;
} KeyRec;

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

int cmp_keyrec(const void *a, const void *b) {
    const KeyRec *A = a;
    const KeyRec *B = b;
    if (A->key < B->key) return -1;
    if (A->key > B->key) return 1;
    return 0;
}

int main(int argc, char **argv) {
    const char *spfile = (argc > 1) ? argv[1] : DEFAULT_SP;
    int indexNo = (argc > 2) ? atoi(argv[2]) : 3;
    int fieldIndex = (argc > 3) ? atoi(argv[3]) : 1;

    printf("=== Bulk load (sorted insert) from %s -> student.%d ===\n", spfile, indexNo);

    int spfd = SP_OpenFile(spfile);
    if (spfd < 0) { perror("SP_OpenFile"); return 1; }

    /* First pass: collect all keys */
    SP_Scan scan;
    SP_ScanInit(&scan, spfd);
    char *rec; int rlen; SP_RecId rid;
    long n = 0;
    /* allocate initial array, grow if needed */
    size_t alloc = 10000;
    KeyRec *arr = malloc(sizeof(KeyRec) * alloc);
    if (!arr) { perror("malloc"); return 1; }
    while (SP_ScanNext(&scan, &rec, &rlen, &rid) == 0) {
        if (n >= (long)alloc) { alloc *= 2; arr = realloc(arr, sizeof(KeyRec) * alloc); if (!arr) { perror("realloc"); return 1; } }
        int key = extract_key_from_record(rec, fieldIndex);
        arr[n].key = key;
        arr[n].recId = (int)rid;
        n++;
        free(rec);
        if (n % 5000 == 0) { printf("."); fflush(stdout); }
    }
    SP_ScanClose(&scan);
    printf("\nCollected %ld keys. Sorting...\n", n);

    qsort(arr, n, sizeof(KeyRec), cmp_keyrec);
    printf("Sort done. Now inserting in sorted order.\n");

    /* create index */
    if (AM_CreateIndex("student", indexNo, 'i', 4) != AME_OK) AM_PrintError("AM_CreateIndex");

    /* measure start */
    clock_t tstart = clock();
    unsigned long beforeLogical = PFbufferPool.logicalPageRequests;
    unsigned long beforePhysReads = PFbufferPool.physicalReads;
    unsigned long beforePhysWrites = PFbufferPool.physicalWrites;

    long inserted = 0;
    char valbuf[4];
    for (long i = 0; i < n; i++) {
        memcpy(valbuf, &arr[i].key, 4);
        int amFd = PF_OpenFile("student.3");
        if (amFd < 0) {
            if (AM_InsertEntry(0, 'i', 4, valbuf, arr[i].recId) != AME_OK) AM_PrintError("AM_InsertEntry");
        } else {
            if (AM_InsertEntry(amFd, 'i', 4, valbuf, arr[i].recId) != AME_OK) AM_PrintError("AM_InsertEntry");
            PF_CloseFile(amFd);
        }
        inserted++;
        if (inserted % 5000 == 0) { printf("."); fflush(stdout); }
    }

    clock_t tend = clock();
    double seconds = (double)(tend - tstart) / CLOCKS_PER_SEC;
    unsigned long logicalDiff = PFbufferPool.logicalPageRequests - beforeLogical;
    unsigned long physReadsDiff = PFbufferPool.physicalReads - beforePhysReads;
    unsigned long physWritesDiff = PFbufferPool.physicalWrites - beforePhysWrites;

    printf("\nInserted %ld sorted entries in %.2f sec\n", inserted, seconds);
    printf("LogicalPageRequests=%lu physicalReads=%lu physicalWrites=%lu\n",
           logicalDiff, physReadsDiff, physWritesDiff);

    /* CSV */
    FILE *csv = fopen(OUTCSV, "w");
    if (csv) {
        fprintf(csv, "method,records,time_sec,logicalReq,physReads,physWrites\n");
        fprintf(csv, "bulk_load_sorted,%ld,%.4f,%lu,%lu,%lu\n",
                inserted, seconds, logicalDiff, physReadsDiff, physWritesDiff);
        fclose(csv);
    }

    free(arr);
    SP_CloseFile(spfd);
    printf("Results written to %s\n", OUTCSV);
    return 0;
}
