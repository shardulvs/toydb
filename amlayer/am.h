typedef struct am_leafheader
	{
		char pageType;
		int nextLeafPage;
		short recIdPtr;
		short keyPtr;
		short freeListPtr;
		short numinfreeList;
		short attrLength;
		short numKeys;
		short maxKeys;
	}  AM_LEAFHEADER; /* Header for a leaf page */

typedef struct am_intheader 
	{
		char pageType;
		short numKeys;
		short maxKeys;
		short attrLength;
	}	AM_INTHEADER ; /* Header for an internal node */

extern int AM_RootPageNum; /* The page number of the root */
extern int AM_LeftPageNum; /* The page Number of the leftmost leaf */
extern int AM_Errno; /* last error in AM layer */
//conflicting with stdlib probably
/* extern char *calloc();
extern char *malloc(); */

# define AM_Check if (errVal != PFE_OK) {AM_Errno = AME_PF; return(AME_PF) ;}
# define AM_si sizeof(int)
# define AM_ss sizeof(short)
# define AM_sl sizeof(AM_LEAFHEADER)
# define AM_sint sizeof(AM_INTHEADER)
# define AM_sc sizeof(char)
# define AM_sf sizeof(float)
# define AM_NOT_FOUND 0 /* Key is not in tree */
# define AM_FOUND 1 /* Key is in tree */
# define AM_NULL 0 /* Null pointer for lists in a page */
# define AM_MAX_FNAME_LENGTH 80
# define AM_NULL_PAGE -1 
# define FREE 0 /* Free is chosen to be zero because C initialises all 
	   variablesto zero and we require that our scan table be initialised */
# define FIRST 1 
# define BUSY 2
# define LAST 3
# define OVER 4
# define ALL 0
# define EQUAL 1
# define LESS_THAN 2
# define GREATER_THAN 3
# define LESS_THAN_EQUAL 4
# define GREATER_THAN_EQUAL 5
# define NOT_EQUAL 6
# define MAXSCANS 20
# define AM_MAXATTRLENGTH 256


# define AME_OK 0
# define AME_INVALIDATTRLENGTH -1
# define AME_NOTFOUND -2
# define AME_PF -3
# define AME_INTERROR -4
# define AME_INVALID_SCANDESC -5
# define AME_INVALID_OP_TO_SCAN -6
# define AME_EOF -7
# define AME_SCAN_TAB_FULL -8
# define AME_INVALIDATTRTYPE -9
# define AME_FD -10
# define AME_INVALIDVALUE -11


void AM_Compact(
int low,
int high,
char *pageBuf,
char *tempPage,
AM_LEAFHEADER *header
);
int AM_InsertintoLeaf(
char *pageBuf,/* buffer where the leaf page resides */
int attrLength,
char *value,/* attribute value to be inserted*/
int recId,/* recid of the attribute to be inserted */
int index,/* index where key is to be inserted */
int status/* Whether key is a new key or an old key */
);
void AM_FillRootPage(
char *pageBuf,/* buffer to new root */
int pageNum1, int pageNum2,/* pagenumbers of it;s two children*/
char *value, /* attr value to be inserted */
short attrLength, short maxKeys /* some info about the header */
);
void AM_topofStack(
int *pageNum,
int *offset
);
void AM_PopStack();
void AM_AddtoIntPage(
char *pageBuf,
char *value, /* value to be added to the node */
int pageNum, /* page number of child to be inserted */
int offset, /* place where key is to be inserted */
AM_INTHEADER *header
);

void AM_SplitIntNode(
char *pageBuf,/* internal node to be split */
char *pbuf1,char *pbuf2, /* the buffers for the two halves */
char *value, /*  pointer to key to be added and to be returned to parent*/
AM_INTHEADER *header,
int pageNum,int offset
);
int AM_Search(
int fileDesc,
char attrType,
int attrLength,
char *value,
int *pageNum, /* page number of page where key is present or can be inserted*/
char **pageBuf, /* pointer to buffer in memory where leaf page corresponding                                                        to pageNum can be found */
int *indexPtr /* pointer to index in leaf where key is present or 
                                                            can be inserted */
);
void AM_EmptyStack();
int AM_SplitLeaf(
int fileDesc, /* file descriptor */
char *pageBuf, /* pointer to buffer */
int *pageNum, /* pagenumber of new leaf created */
int attrLength, 
int recId,
char *value, /* attribute value for insert */

int status, /* Whether key was found or not in the tree */
int index, /* place where key is to be inserted */
char *key /* returns the key to be filled in the parent */
);
int AM_AddtoParent(
int fileDesc,
int pageNum, /* page Number to be added to parent */
char *value, /*  pointer to attribute value to be added - 
                 gives back the attribute value to be added to it's parent*/
int attrLength
);
void AM_InsertToLeafFound(
char *pageBuf,
int recId,
int index,
AM_LEAFHEADER *header
);
void AM_InsertToLeafNotFound(
char *pageBuf,
char *value,
int recId,
int index,
AM_LEAFHEADER *header
);
int AM_PrintLeafNode(
char *pageBuf,
char attrType
);
void AM_PrintAttr(
char *bufPtr,
char attrType,
int attrLength
);
void AM_PrintLeafKeys(
char *pageBuf,
char attrType
);
int GetLeftPageNum(
int fileDesc
);

int AM_Compare(
char *bufPtr,
char attrType,
char *valPtr,
int attrLength
);
int AM_BinSearch(
char *pageBuf, /* buffer where the page is found */
char attrType, 
int attrLength,
char *value, /* attribute value for which search is called */
int *indexPtr,  
AM_INTHEADER *header
);
void AM_PushStack(
int pageNum,
int offset
);
int AM_SearchLeaf(
char *pageBuf, /* buffer where the leaf page resides */
char attrType,
int attrLength,
char *value, /* attribute value to be compared with */
int *indexPtr,/* pointer to the index where key is found or can be inserted */
AM_LEAFHEADER *header
);
int AM_CreateIndex(
char *fileName,/* Name of indexed file */
int indexNo, /*number of this index for file */
char attrType, /* 'c' for char ,'i' for int ,'f' for float */
int attrLength /* 4 for 'i' or 'f', 1-255 for 'c' */
);
int AM_InsertEntry(
int fileDesc, /* file Descriptor */
char attrType, /* 'i' or 'c' or 'f' */
int attrLength, /* 4 for 'i' or 'f', 1-255 for 'c' */
char *value, /* value to be inserted */ 
int recId /* recId to be inserted */
);
int AM_DeleteEntry(
int fileDesc, /* file Descriptor */
char attrType, /* 'c' , 'i' or 'f' */
int attrLength, /* 4 for 'i' or 'f' , 1-255 for 'c' */
char *value,/* Value of key whose corr recId is to be deleted */
int recId /* id of the record to delete */
);
int AM_OpenIndexScan(
int fileDesc, /* file Descriptor */

char attrType, /* 'i' or 'c' or 'f' */
int attrLength, /* 4 for 'i' or 'f' , 1-255 for 'c' */
int op, /* operator for comparison */
char *value /* value for comparison */
);
int AM_CloseIndexScan(
int scanDesc/* scan Descriptor*/
);
int AM_FindNextEntry(
int scanDesc/* index scan descriptor */
);
int AM_DestroyIndex(
char *fileName,/* name of indexed file */
int indexNo /* number of this index for file */
);
