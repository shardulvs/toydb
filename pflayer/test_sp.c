#include "splayer.h"
#include "pftypes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define STUDENT_FILE "student.txt"
#define SPL_FILE "sp_student.dat"
#define CSV_OUT "sp_results.csv"

#define MAX_LINE_LEN 4096

/* Read student file line-by-line and return malloc'd line (trimmed newline).
   Assumes each non-empty line is a record. */
static char *read_next_record(FILE *f) {
    char buf[MAX_LINE_LEN];
    if (!fgets(buf, sizeof(buf), f)) return NULL;
    size_t l = strlen(buf);
    while (l > 0 && (buf[l-1] == '\n' || buf[l-1] == '\r')) { buf[--l] = 0; }
    if (l == 0) return strdup("");
    return strdup(buf);
}

/* Simulate static fixed-length storage metrics:
   For a given max_rec_len, compute how many fixed records per page and wasted bytes.
   Returns pages_needed and sets *wasted_total (sum of wasted bytes across pages). */
static int simulate_static(
        long total_records,
        int max_rec_len,
        int *out_pages,
        long *out_wasted,
        double *out_util)
{
    if (max_rec_len <= 0) return -1;

    int per_page = PF_PAGE_SIZE / max_rec_len;
    if (per_page <= 0) return -1;

    int pages = (total_records + per_page - 1) / per_page;

    long total_capacity = (long)pages * PF_PAGE_SIZE;
    long used_bytes = (long)total_records * max_rec_len;
    long wasted = total_capacity - used_bytes;

    double util = (double)used_bytes / (double)total_capacity * 100.0;

    *out_pages = pages;
    *out_wasted = wasted;
    *out_util = util;

    return 0;
}

int main() {
    FILE *sf = fopen(STUDENT_FILE, "r");
    if (!sf) { perror("student.txt"); return 1; }

    /* Create slotted file */
    PF_DestroyFile(SPL_FILE);
    if (SP_CreateFile(SPL_FILE) != PFE_OK) { fprintf(stderr,"SP_CreateFile failed\n"); return 1; }
    int fd = SP_OpenFile(SPL_FILE);
    if (fd < 0) { fprintf(stderr,"SP_OpenFile failed\n"); return 1; }

    printf("Inserting records from %s into slotted-page file %s ...\n", STUDENT_FILE, SPL_FILE);

    long total_records = 0;
    long total_bytes = 0;

    char *rec;
    while ((rec = read_next_record(sf)) != NULL) {
        int len = strlen(rec);
        /* use entire line as record */
        SP_RecId rid;
        if (SP_InsertRecord(fd, rec, len, &rid) != 0) {
            fprintf(stderr,"SP_InsertRecord failed for rec %ld\n", total_records);
            free(rec);
            break;
        }
        total_records++;
        total_bytes += len;
        free(rec);
    }
    fclose(sf);

    /* compute utilization for slotted */
    int pages_used;
    long used_bytes;
    double util = SP_ComputeSpaceUtilization(fd, &pages_used, &used_bytes);

    printf("Slotted-page: records=%ld bytes=%ld pages=%d utilization=%.2f%%\n",
           total_records, used_bytes, pages_used, util);

    /* write CSV header */
    FILE *csv = fopen(CSV_OUT, "w");
    if (!csv) { perror("csv"); return 1; }
    fprintf(csv, "mode,total_records,total_bytes,pages,util_percent,static_max_rec_len,static_pages,static_wasted_bytes\n");

    /* record slotted result row (static fields empty) */
    fprintf(csv, "slotted,%ld,%ld,%d,%.2f, , , \n",
            total_records, used_bytes, pages_used, util);

    /* static simulations for several max_rec_len values */
    int sim_lengths[] = {32, 64, 128, 256, 512, 1024};
    int nsim = sizeof(sim_lengths)/sizeof(sim_lengths[0]);

    for (int i = 0; i < nsim; i++) 
    {
        /* --- Correct static fixed-length simulation --- */
        int maxlen = sim_lengths[i];
        int per_page = PF_PAGE_SIZE / maxlen;

        if (per_page <= 0) {
            printf("Static simulation failed for maxlen=%d\n", maxlen);
            continue;
        }

        /* total records stored using fixed-length maxlen */
        long used_bytes = (long)total_records * maxlen;

        /* pages needed */
        int static_pages = (total_records + per_page - 1) / per_page;

        /* total capacity across these pages */
        long capacity_bytes = (long)static_pages * PF_PAGE_SIZE;

        /* wasted bytes (always >= 0) */
        long wasted = capacity_bytes - used_bytes;

        /* utilization = used / capacity */
        double util_static = 0.0;
        if (capacity_bytes > 0)
            util_static = (double)used_bytes / (double)capacity_bytes * 100.0;

        /* Print results */
        printf("Static (maxlen=%d): pages=%d wasted=%ld util=%.2f%%\n",
               maxlen, static_pages, wasted, util_static);

        /* Write CSV row */
        fprintf(csv,
                "static,%ld,%ld,%d,%.2f,%d,%d,%ld\n",
                total_records,              /* number of records */
                used_bytes,                 /* total bytes used (fixed size) */
                static_pages,               /* pages needed */
                util_static,                /* utilization percentage */
                maxlen,                     /* fixed record size tested */
                static_pages,               /* pages again for clarity */
                wasted                      /* wasted bytes */
        );
    }

    fclose(csv);

    SP_CloseFile(fd);

    printf("Results saved to %s\n", CSV_OUT);

    return 0;
}
