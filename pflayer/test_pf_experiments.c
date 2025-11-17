#include "pf.h"
#include "pftypes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define TESTFILE "pf_auto_testfile.dat"
#define CSVFILE  "pf_results.csv"

/* Number of ops per experiment */
#define OPS_PER_RUN 50000
#define MAXPAGE     50

/* Runs one experiment */
void run_workload(int readPct, int ops, int maxPage, FILE *csv)
{
    int fd;
    int pageNum;
    char *pageBuf;
    int i;

    printf("\n====================================================\n");
    printf(" Running workload: %d ops | %d%% reads | %d%% writes\n",
            ops, readPct, 100 - readPct);
    printf("====================================================\n");

    fflush(stdout);

    /* Reset PF stats */
    PFbufferPool.logicalPageRequests = 0;
    PFbufferPool.logicalPageHits     = 0;
    PFbufferPool.physicalReads       = 0;
    PFbufferPool.physicalWrites      = 0;

    /* Always recreate test file */
    PF_DestroyFile(TESTFILE);
    if (PF_CreateFile(TESTFILE) != PFE_OK) {
        PF_PrintError("CreateFile");
        exit(1);
    }

    fd = PF_OpenFile(TESTFILE);
    if (fd < 0) {
        PF_PrintError("OpenFile");
        exit(1);
    }

    /* Pre-allocate pages */
    for (i = 0; i < maxPage; i++) {
        if (PF_AllocPage(fd, &pageNum, &pageBuf) != PFE_OK) {
            PF_PrintError("AllocPage");
            exit(1);
        }
        memset(pageBuf, 0, PF_PAGE_SIZE);
        PF_UnfixPage(fd, pageNum, TRUE);
    }

    /* Random ops */
    for (i = 0; i < ops; i++) {
        int r = rand() % 100;
        int target = rand() % maxPage;

        if (r < readPct) {
            /* READ */
            if (PF_GetThisPage(fd, target, &pageBuf) != PFE_OK) {
                PF_PrintError("Read Get");
                exit(1);
            }
            PF_UnfixPage(fd, target, FALSE);
        } else {
            /* WRITE */
            if (PF_GetThisPage(fd, target, &pageBuf) != PFE_OK) {
                PF_PrintError("Write Get");
                exit(1);
            }
            pageBuf[0] = (char)(target & 0xFF);
            pageBuf[1] = (char)(i & 0xFF);
            PF_UnfixPage(fd, target, TRUE);
        }

        /* Print a dot every 5000 ops for user feedback */
        if (i % 5000 == 0) {
            printf(".");
            fflush(stdout);
        }
    }

    PF_CloseFile(fd);

    printf("\n---- Results for %d%% Reads ----\n", readPct);
    PF_DumpStats();
    printf("-----------------------------------\n");

    /* Write CSV row */
    fprintf(csv, "%d,%d,%d,%lu,%lu,%lu,%lu\n",
            readPct,
            ops,
            maxPage,
            PFbufferPool.logicalPageRequests,
            PFbufferPool.logicalPageHits,
            PFbufferPool.physicalReads,
            PFbufferPool.physicalWrites);

    fflush(csv);
}

int main()
{
    srand(time(NULL));

    printf("Initializing PF System...\n");
    PF_Init();
    PF_InitWithOptions(20, PF_REPLACEMENT_LRU);  /* 20 frames, LRU */

    FILE *csv = fopen(CSVFILE, "w");
    if (!csv) {
        perror("fopen");
        return 1;
    }

    /* Write CSV header */
    fprintf(csv, "readPct,ops,maxPage,logicalReq,hits,physicalReads,physicalWrites\n");

    /* Loop read percentages 100, 90, 80, ..., 0 */
    int percentages[] = {100,90,80,70,60,50,40,30,20,10,0};

    for (int i = 0; i < 11; i++) {
        run_workload(percentages[i], OPS_PER_RUN, MAXPAGE, csv);
    }

    fclose(csv);

    printf("\n====================================================\n");
    printf(" All experiments completed.\n");
    printf(" Results stored in: %s\n", CSVFILE);
    printf("====================================================\n\n");

    return 0;
}
