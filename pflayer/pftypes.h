/* pftypes.h: declarations for Paged File interface */
#pragma once
#include "pf.h"

/**************************** File Page Decls *********************/
/* Each file contains a header, which is a integer pointing
to the first free page, or -1 if no more free pages in the file.
Followed by this header are the file pages as declared in struct PFfpage */
typedef struct PFhdr_str {
	int	firstfree;	/* first free page in the linked list of
				free pages */
	int	numpages;	/* # of pages in the file */
} PFhdr_str;

#define PF_HDR_SIZE sizeof(PFhdr_str)	/* size of file header */

/* actual page struct to be written onto the file */
#define PF_PAGE_LIST_END	-1	/* end of list of free pages */
#define PF_PAGE_USED		-2	/* page is being used */
typedef struct PFfpage {
	int nextfree;	/* page number of next free page in the linked
			list of free pages, or PF_PAGE_LIST_END if
			end of list, or PF_PAGE_USED if this page is not free */
	char pagebuf[PF_PAGE_SIZE];	/* actual page data */
} PFfpage;

/*************************** Opened File Table **********************/
#define PF_FTAB_SIZE	20	/* size of open file table */

/* open file table entry */
typedef struct PFftab_ele {
	char *fname;	/* file name, or NULL if entry not used */
	int unixfd;	/* unix file descriptor*/
	PFhdr_str hdr;	/* file header */
	short hdrchanged; /* TRUE if file header has changed */
} PFftab_ele;

/************************** Buffer Page Decls *********************/
#define PF_MAX_BUFS	20	/* max # of buffers */

/* buffer page decl */
typedef struct PFbpage {
	struct PFbpage *nextpage;	/* next in the linked list of
					buffer page */
	struct PFbpage *prevpage;	/* previous in the linked list
					of buffer pages */
	unsigned short	dirty:1,		/* TRUE if page is dirty */
		fixed:1;		/* TRUE if page is fixed in buffer*/
	int	page;			/* page number of this page */
	int	fd;			/* file desciptor of this page */
	PFfpage fpage; /* page data from the file */
} PFbpage;



/******************** Hash Table Decls ****************************/
#define PF_HASH_TBL_SIZE	20	/* size of PF hash table */

/* Hash table bucket entries*/
typedef struct PFhash_entry {
	struct PFhash_entry *nextentry; /* next hash table element, or NULL */
	struct PFhash_entry *preventry;/*previous hash table element,or NULL*/
	int fd;		/* file descriptor */
	int page;	/* page number */
	struct PFbpage *bpage; /* pointer to buffer holding this page */
} PFhash_entry;

/* Hash function for hash table */
#define PFhash(fd,page) (((fd)+(page)) % PF_HASH_TBL_SIZE)

/******************* Interface functions from Hash Table ****************/
void PFhashInit();
PFbpage *PFhashFind();
int PFhashInsert();
int PFhashDelete();
void PFhashPrint();

/****************** Interface functions from Buffer Manager *************/
// int PFbufGet();
int PFbufUnfix(int fd,      /* file descriptor */
               int pagenum, /* page number */
               int dirty    /* TRUE if page is dirty */
);

int PFbufUsed(int fd,     /* file descriptor */
              int pagenum /* page number */
);

int PFbufAlloc(int fd,          /* file descriptor */
               int pagenum,     /* page number */
               PFfpage **fpage, /* pointer to file page */
               int (*writefcn)(int, int, PFfpage*));

int PFbufReleaseFile(
    int fd,                              /* file descriptor */
    int (*writefcn)(int, int, PFfpage *) /* function to write a page of file */
);

int PFbufGet(int fd,          /* file descriptor */
             int pagenum,     /* page number */
             PFfpage **fpage, /* pointer to pointer to file page */
             int (*readfcn)(int, int, PFfpage *), /* function to read a page */
             int (*writefcn)(int, int, PFfpage *) /* function to write a page */
);

/************ More declarations that the compiler needs to see **********/
PFbpage *PFhashFind(int fd, int page);
int PFhashInsert(int fd, int page, PFbpage* bpage);
int PFhashDelete(int fd, int page);
int PF_CreateFile(char* fname);
int PF_OpenFile(char* fname);
int PF_DisposePage(int fd, int pagenum);
int PF_CloseFile(int fd);
int	PF_DestroyFile(char* fname);
int PF_AllocPage(int fd, int* pagenum, char** pagebuf);
int PF_UnfixPage(int fd, int pagenum, int dirty);
int PF_GetThisPage(int fd, int pagenum, char** pagebuf);
void PFbufPrint();
int PF_GetNextPage(int fd, int* pagenum, char** pagebuf);
void PF_PrintError(char* s);
