/* pf.h: externs and error codes for Paged File Interface*/
#pragma once

#ifndef TRUE
#define TRUE 1		
#endif
#ifndef FALSE
#define FALSE 0
#endif

/************** Error Codes *********************************/
#define PFE_OK		0	/* OK */
#define PFE_NOMEM	-1	/* no memory */
#define PFE_NOBUF	-2	/* no buffer space */
#define PFE_PAGEFIXED 	-3	/* page already fixed in buffer */
#define PFE_PAGENOTINBUF -4	/* page to be unfixed is not in the buffer */
#define PFE_UNIX	-5	/* unix error */
#define PFE_INCOMPLETEREAD -6	/* incomplete read of page from file */
#define PFE_INCOMPLETEWRITE -7	/* incomplete write of page to file */
#define PFE_HDRREAD	-8	/* incomplete read of header from file */
#define PFE_HDRWRITE	-9	/* incomplte write of header to file */
#define PFE_INVALIDPAGE -10	/* invalid page number */
#define PFE_FILEOPEN	-11	/* file already open */
#define	PFE_FTABFULL	-12	/* file table is full */
#define PFE_FD		-13	/* invalid file descriptor */
#define PFE_EOF		-14	/* end of file */
#define PFE_PAGEFREE	-15	/* page already free */
#define PFE_PAGEUNFIXED	-16	/* page already unfixed */

/* Internal error: please report to the TA */
#define PFE_PAGEINBUF	-17	/* new page to be allocated already in buffer */
#define PFE_HASHNOTFOUND -18	/* hash table entry not found */
#define PFE_HASHPAGEEXIST -19	/* page already exist in hash table */


/* page size */
#define PF_PAGE_SIZE	4096

/* externs from the PF layer */
extern int PFerrno;		/* error number of last error */
void PF_Init();
void PF_PrintError(char* s);

#define PF_REPLACEMENT_LRU 0
#define PF_REPLACEMENT_MRU 1

typedef struct PF_Frame {
    int fileDesc;         /* which file this frame belongs to */
    int pageNum;          /* page number or -1 if free */
    char *data;           /* pointer to page data (PF_PAGE_SIZE) */
    int dirty;            /* TRUE if modified */
    int fixedCount;       /* number of pins */
    struct PF_Frame *prev, *next; /* for LRU/MRU doubly linked list */
} PF_Frame;


typedef struct PF_BufferPool {
    PF_Frame *frames; /* array of frames */
    int poolSize;     /* number of frames */
    int replacement;  /* PF_REPLACEMENT_LRU / PF_REPLACEMENT_MRU */
    PF_Frame *lru_head; /* head = MRU or LRU depending on convention */
    PF_Frame *lru_tail;
    /* Hash map from (fileDesc,pageNum) -> frame index (use simple chaining or fixed hash) */
    /* Stats */
    unsigned long logicalPageRequests;
    unsigned long logicalPageHits;
    unsigned long physicalReads;
    unsigned long physicalWrites;
    unsigned long pageAllocations;
} PF_BufferPool;

void PF_InitWithOptions(int poolSize, int replacementPolicy);
void PFbufInitPool(int poolSize);
extern struct PF_BufferPool PFbufferPool;
void PF_DumpStats();
