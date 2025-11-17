/* splayer.c
 *
 * Slotted-page layer built on top of PF layer.
 * Patch: use sizeof(...) for header/slot sizes, fix free-space accounting,
 *       avoid pinning many pages during scans, and robust insertion logic.
 */

#include "splayer.h"
#include "pftypes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* Page header layout */
typedef struct {
    uint32_t magic;       /* validation */
    uint16_t slot_count;  /* number of slots allocated */
    uint16_t free_offset; /* offset where next record will be placed (grows downward) */
    uint16_t free_space;  /* free bytes available */
} SP_PageHeader;

typedef struct {
    int16_t offset; /* -1 = deleted */
    int16_t length;
} SP_SlotEntry;

/* Use sizeof() everywhere for portability */
#define SP_HEADER_SIZE (sizeof(SP_PageHeader))
#define SP_SLOT_SIZE   (sizeof(SP_SlotEntry))

#define SP_MAGIC_VAL 0x53504C54 /* "SPLT" */

static void sp_init_page(char *pagebuf) {
    SP_PageHeader hdr;
    hdr.magic = SP_MAGIC_VAL;
    hdr.slot_count = 0;
    hdr.free_offset = (uint16_t)PF_PAGE_SIZE; /* data grows downward from end */
    hdr.free_space = (uint16_t)(PF_PAGE_SIZE - SP_HEADER_SIZE);
    memcpy(pagebuf, &hdr, SP_HEADER_SIZE);
}

/* Helper to read header from page buffer */
static void sp_read_header(char *pagebuf, SP_PageHeader *hdr) {
    memcpy(hdr, pagebuf, SP_HEADER_SIZE);
}

/* Helper to write header */
static void sp_write_header(char *pagebuf, const SP_PageHeader *hdr) {
    memcpy(pagebuf, hdr, SP_HEADER_SIZE);
}

/* Get pointer to slot i in given page buffer */
static SP_SlotEntry *sp_slot_ptr(char *pagebuf, int slotIndex) {
    return (SP_SlotEntry *)(pagebuf + SP_HEADER_SIZE + slotIndex * SP_SLOT_SIZE);
}

/* Utility: check if page is a valid slotted page */
static int sp_is_valid(char *pagebuf) {
    SP_PageHeader hdr;
    sp_read_header(pagebuf, &hdr);
    return hdr.magic == SP_MAGIC_VAL;
}

/* Create file wrapper */
int SP_CreateFile(const char *fileName) {
    return PF_CreateFile((char *)fileName);
}

int SP_DestroyFile(const char *fileName) {
    return PF_DestroyFile((char *)fileName);
}

int SP_OpenFile(const char *fileName) {
    return PF_OpenFile((char *)fileName);
}

int SP_CloseFile(int fd) {
    return PF_CloseFile(fd);
}

/* Find a page with enough space; returns pageNum in *pageNum and pageBuf fixed.
   Caller must PF_UnfixPage(pageNum, ...) when done. */
static int sp_find_page_for_insert(int fd, int rec_len, int *outPageNum, char **outPageBuf) {
    int err;
    int pageNum;
    char *pagebuf;

    /* scan pages */
    err = PF_GetFirstPage(fd, &pageNum, &pagebuf);
    if (err == PFE_EOF) {
        /* empty file: allocate new page */
        if (PF_AllocPage(fd, &pageNum, &pagebuf) != PFE_OK) return -1;
        sp_init_page(pagebuf);
        *outPageNum = pageNum;
        *outPageBuf = pagebuf;
        return 0;
    } else if (err != PFE_OK) {
        return -1;
    }

    /* Iterate pages; unfix each page before moving to next to avoid pinning many frames */
    while (1) {
        SP_PageHeader hdr;
        sp_read_header(pagebuf, &hdr);

        /* Determine whether this page can accommodate the record.
           If there is a deleted slot we only need rec_len bytes.
           Otherwise we need rec_len + SP_SLOT_SIZE */
        int found_deleted = 0;
        for (int i = 0; i < hdr.slot_count; i++) {
            SP_SlotEntry *s = sp_slot_ptr(pagebuf, i);
            if (s->offset == -1) { found_deleted = 1; break; }
        }
        int needed = rec_len + (found_deleted ? 0 : SP_SLOT_SIZE);

        if ((int)hdr.free_space >= needed) {
            *outPageNum = pageNum;
            *outPageBuf = pagebuf;
            return 0;
        }

        /* move to next page: unfix current and request next */
        int prevPage = pageNum;
        if (PF_UnfixPage(fd, prevPage, FALSE) != PFE_OK) return -1;

        int rc = PF_GetNextPage(fd, &pageNum, &pagebuf);
        if (rc == PFE_EOF) break;
        if (rc != PFE_OK) return -1;
    }

    /* no existing page had enough space -> allocate new page */
    if (PF_AllocPage(fd, &pageNum, &pagebuf) != PFE_OK) return -1;
    sp_init_page(pagebuf);
    *outPageNum = pageNum;
    *outPageBuf = pagebuf;
    return 0;
}

/* Insert record */
int SP_InsertRecord(int fd, const char *data, int len, SP_RecId *recId) {
    if (len <= 0 || len > PF_PAGE_SIZE - SP_HEADER_SIZE - SP_SLOT_SIZE) return -1;

    int pageNum;
    char *pagebuf;
    if (sp_find_page_for_insert(fd, len, &pageNum, &pagebuf) != 0) return -1;

    SP_PageHeader hdr;
    sp_read_header(pagebuf, &hdr);

    /* find slot index: either reuse deleted slot or append new */
    int slotIndex = -1;
    int reuse_slot = 0;
    for (int i = 0; i < hdr.slot_count; i++) {
        SP_SlotEntry *s = sp_slot_ptr(pagebuf, i);
        if (s->offset == -1) { slotIndex = i; reuse_slot = 1; break; }
    }
    if (slotIndex == -1) {
        slotIndex = hdr.slot_count;
        hdr.slot_count++; /* we will add a new slot */
    }

    /* compute slot_dir_size after slot_count change */
    size_t slot_dir_size = (size_t)hdr.slot_count * SP_SLOT_SIZE;
    /* compute needed bytes: record data plus slot dir space only if we added a new slot */
    int needed = len + (reuse_slot ? 0 : SP_SLOT_SIZE);

    if ((int)hdr.free_space < needed) {
        /* Should not normally happen because sp_find_page_for_insert checked again,
           but double-check here */
        PF_UnfixPage(fd, pageNum, FALSE);
        return -1;
    }

    /* allocate space at free_offset */
    hdr.free_offset = (uint16_t)(hdr.free_offset - len);
    int data_off = hdr.free_offset;
    memcpy(pagebuf + data_off, data, len);

    /* update slot */
    SP_SlotEntry se;
    se.offset = (int16_t)data_off;
    se.length = (int16_t)len;
    memcpy(sp_slot_ptr(pagebuf, slotIndex), &se, SP_SLOT_SIZE);

    /* update free space: subtract record bytes and slot directory occupancy */
    hdr.free_space = (uint16_t)(hdr.free_offset - (SP_HEADER_SIZE + slot_dir_size));

    /* write header back and unfix page (dirty) */
    sp_write_header(pagebuf, &hdr);

    if (PF_UnfixPage(fd, pageNum, TRUE) != PFE_OK) return -1;

    if (recId) *recId = ((unsigned int)pageNum << 16) | (unsigned int)slotIndex;
    return 0;
}

/* Helper to decode recId */
static void decode_recId(SP_RecId rid, int *pageNum, int *slotIndex) {
    *pageNum = (int)(rid >> 16);
    *slotIndex = (int)(rid & 0xFFFF);
}

/* Get record */
int SP_GetRecord(int fd, SP_RecId recId, char *buf, int *len) {
    int pageNum, slotIndex;
    decode_recId(recId, &pageNum, &slotIndex);

    char *pagebuf;
    if (PF_GetThisPage(fd, pageNum, &pagebuf) != PFE_OK) return -1;

    SP_PageHeader hdr;
    sp_read_header(pagebuf, &hdr);
    if (slotIndex >= hdr.slot_count) {
        PF_UnfixPage(fd, pageNum, FALSE);
        return -1;
    }
    SP_SlotEntry *s = sp_slot_ptr(pagebuf, slotIndex);
    if (s->offset == -1) {
        PF_UnfixPage(fd, pageNum, FALSE);
        return -1; /* deleted */
    }
    if (len) *len = s->length;
    if (buf) memcpy(buf, pagebuf + s->offset, s->length);

    PF_UnfixPage(fd, pageNum, FALSE);
    return 0;
}

/* Delete record: mark slot offset = -1 and increase free_space by length (lazy) */
int SP_DeleteRecord(int fd, SP_RecId recId) {
    int pageNum, slotIndex;
    decode_recId(recId, &pageNum, &slotIndex);

    char *pagebuf;
    if (PF_GetThisPage(fd, pageNum, &pagebuf) != PFE_OK) return -1;

    SP_PageHeader hdr;
    sp_read_header(pagebuf, &hdr);
    if (slotIndex >= hdr.slot_count) {
        PF_UnfixPage(fd, pageNum, FALSE);
        return -1;
    }
    SP_SlotEntry *s = sp_slot_ptr(pagebuf, slotIndex);
    if (s->offset == -1) {
        PF_UnfixPage(fd, pageNum, FALSE);
        return -1; /* already deleted */
    }

    /* Mark deleted and reclaim length to free_space (slot directory remains) */
    int freed = s->length;
    s->offset = -1;
    s->length = 0;

    /* update header's free_space */
    size_t slot_dir_size = (size_t)hdr.slot_count * SP_SLOT_SIZE;
    hdr.free_space = (uint16_t)(hdr.free_space + freed);
    sp_write_header(pagebuf, &hdr);

    if (PF_UnfixPage(fd, pageNum, TRUE) != PFE_OK) return -1;

    return 0;
}

/* Compact a single page: relocate records into contiguous region and update slots */
int SP_CompactPage(int fd, int pageNum) {
    char *pagebuf;
    if (PF_GetThisPage(fd, pageNum, &pagebuf) != PFE_OK) return -1;

    SP_PageHeader hdr;
    sp_read_header(pagebuf, &hdr);

    /* allocate temporary snapshot of page content */
    char *tmp = malloc(PF_PAGE_SIZE);
    if (!tmp) { PF_UnfixPage(fd, pageNum, FALSE); return -1; }
    memcpy(tmp, pagebuf, PF_PAGE_SIZE);

    /* rebuild data: start filling from PF_PAGE_SIZE downward */
    int cur_free = PF_PAGE_SIZE;
    for (int i = 0; i < hdr.slot_count; i++) {
        SP_SlotEntry *s = (SP_SlotEntry *)(tmp + SP_HEADER_SIZE + i * SP_SLOT_SIZE);
        if (s->offset == -1) continue;
        cur_free -= s->length;
        memcpy(pagebuf + cur_free, tmp + s->offset, s->length);
        /* update slot offset in pagebuf */
        SP_SlotEntry new_s;
        new_s.offset = (int16_t)cur_free;
        new_s.length = s->length;
        memcpy(sp_slot_ptr(pagebuf, i), &new_s, SP_SLOT_SIZE);
    }

    hdr.free_offset = (uint16_t)cur_free;
    hdr.free_space = (uint16_t)(hdr.free_offset - (SP_HEADER_SIZE + hdr.slot_count * SP_SLOT_SIZE));
    sp_write_header(pagebuf, &hdr);

    free(tmp);
    if (PF_UnfixPage(fd, pageNum, TRUE) != PFE_OK) return -1;
    return 0;
}

/* Scanner functions */
int SP_ScanInit(SP_Scan *scan, int fd) {
    memset(scan, 0, sizeof(*scan));
    scan->fd = fd;
    scan->initialized = 0;
    scan->curPageNum = -1;
    scan->pageBuf = NULL;
    scan->slotIndex = 0;
    return 0;
}

/* Returns 0 on success with outBuf pointing to freshly malloc'd buffer (caller must free). */
int SP_ScanNext(SP_Scan *scan, char **outBuf, int *outLen, SP_RecId *outRecId) {
    int rc;
    int pageNum;
    char *pagebuf;

    if (!scan->initialized) {
        rc = PF_GetFirstPage(scan->fd, &pageNum, &pagebuf);
        if (rc == PFE_EOF) return -1;
        if (rc != PFE_OK) return -1;
        scan->curPageNum = pageNum;
        scan->pageBuf = pagebuf;
        scan->slotIndex = 0;
        scan->initialized = 1;
    }

    while (1) {
        SP_PageHeader hdr;
        sp_read_header(scan->pageBuf, &hdr);

        for (; scan->slotIndex < hdr.slot_count; scan->slotIndex++) {
            SP_SlotEntry *s = sp_slot_ptr(scan->pageBuf, scan->slotIndex);
            if (s->offset == -1) continue;
            /* found record */
            int len = s->length;
            char *buf = malloc(len);
            if (!buf) return -1;
            memcpy(buf, scan->pageBuf + s->offset, len);
            if (outBuf) *outBuf = buf;
            if (outLen) *outLen = len;
            if (outRecId) *outRecId = ((unsigned int)scan->curPageNum << 16) | (unsigned int)scan->slotIndex;
            scan->slotIndex++;
            return 0;
        }

        /* move to next page: unfix current then fetch next */
        int prevPage = scan->curPageNum;
        if (PF_UnfixPage(scan->fd, prevPage, FALSE) != PFE_OK) return -1;

        rc = PF_GetNextPage(scan->fd, &scan->curPageNum, &scan->pageBuf);
        if (rc == PFE_EOF) {
            scan->initialized = 0;
            return -1;
        }
        if (rc != PFE_OK) return -1;
        scan->slotIndex = 0;
    }
}

/* Close scan */
int SP_ScanClose(SP_Scan *scan) {
    /* ensure current page unfixed */
    if (scan->initialized && scan->curPageNum >= 0) PF_UnfixPage(scan->fd, scan->curPageNum, FALSE);
    scan->initialized = 0;
    return 0;
}

/* Compute utilization */
double SP_ComputeSpaceUtilization(int fd, int *out_pages, long *out_total_bytes) {
    int rc;
    int pageNum;
    char *pagebuf;
    long total_bytes = 0;
    int pages = 0;

    rc = PF_GetFirstPage(fd, &pageNum, &pagebuf);
    if (rc == PFE_EOF) {
        if (out_pages) *out_pages = 0;
        if (out_total_bytes) *out_total_bytes = 0;
        return 0.0;
    }
    if (rc != PFE_OK) return 0.0;

    while (1) {
        SP_PageHeader hdr;
        sp_read_header(pagebuf, &hdr);
        pages++;
        /* sum record lengths */
        long used = 0;
        for (int i = 0; i < hdr.slot_count; i++) {
            SP_SlotEntry *s = sp_slot_ptr(pagebuf, i);
            if (s->offset == -1) continue;
            used += s->length;
        }
        total_bytes += used;

        /* move to next: unfix current then get next page */
        int prevPage = pageNum;
        if (PF_UnfixPage(fd, prevPage, FALSE) != PFE_OK) return 0.0;

        rc = PF_GetNextPage(fd, &pageNum, &pagebuf);
        if (rc == PFE_EOF) break;
        if (rc != PFE_OK) return 0.0;
    }

    if (out_pages) *out_pages = pages;
    if (out_total_bytes) *out_total_bytes = total_bytes;
    double util = 0.0;
    if (pages > 0) {
        util = ((double)total_bytes) / ((double)pages * (double)PF_PAGE_SIZE) * 100.0;
    }
    return util;
}
