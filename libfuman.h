
#define _GNU_SOURCE
#define FUSE_USE_VERSION 26
#include <fuse.h>
#include <time.h>
#include <fuse_lowlevel.h>
#include <fuse_common.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <stdbool.h>
#include <math.h>
#include <errno.h>

#define SZ_BLOCK 512
#define SZ_DIRENTRY 32
#define SZ_CHAINLINK 2
#define MAX_LEN_FILENAME 12
#define INODE_START 2
#define DEBUG 1

typedef struct fileObject fObj;
typedef struct fileBlock fBlk;
typedef struct fileUnitManager fUMan;
typedef struct rootBlock rBlk;
typedef struct directoryFileEntry dFEnt;

//root block of the FAT, 512 bytes
/*	rootBlock
*	Represents all of the information given in the root block,
*	typically the last block of the image.
*	---------------
*	file signature
*	color code
*	blank section
*	BCD timestamp
*	blank section
*	startBlk of FAT
*	len of FAT 
*	lastBlk of dir
*	len of dir
*	file icon
*	user fBlks
*	empty section
*/
struct rootBlock {
	unsigned char signature[16];
	unsigned char color[5];
	unsigned char blank1[27];
	unsigned char BCD[8];
	unsigned char blank2[14];
	unsigned short fat_start;
	unsigned short fat_sz;
	unsigned short end_dir_blk;
	unsigned short dir_sz;
	unsigned short icon;
	unsigned short num_user_blks;
	unsigned char unused[430];
};

//Directory entry, 32bytes
/*
*	file type
*	copy_protection
*	first data block
*	file name
*	BCD timestamp
*	file size(blocks)
*	file header offset
*	empty section
*/
struct directoryFileEntry{
	unsigned char type;
	unsigned char cp_protect;
	unsigned short start_blk;
	unsigned char file_name[MAX_LEN_FILENAME];
	unsigned char BCD[8];
	unsigned short file_sz;
	unsigned short hdr_offt;
	unsigned char empty[4];
};

//fileObject
//"represents a file"
/*
*	type 	- not currently in use
*	active 	- 1(active, not queued)/0(inactive, queued)
*	len 	- number of bytes managed by fileObject
*	fObj_id - id of fileObject
* 	dfe 	- struct directoryFileEntry for fileObject
*	HT_PN	- neighbors in queue OR head/tail of fileBlocks
*/
struct fileObject{
	char type;
	char active;
	unsigned int len;
	int fObj_id;
	dFEnt* dfe;
	void* HT_PN[2];
};

//fileBlock
//"makes up part of a file"
/*
*	type 	- not currently in use
*	active 	- 1(active, not queued)/0(inactive, queued)
* 	fObj_id - id of owner fileObject, -1 if free
*	fBlk_id - id of fileBlock
* 	data 	- pointer to fileBlock data,SZ_BLOCK byte buffer
* 	prev 	- addr of prev fileBlock
* 	next 	- address of next fileBlock
*/
struct fileBlock{
	char type;
	char active;
	int fObj_id;
	int fBlk_id;
	unsigned char* data;
	fBlk** prev;
	fBlk** next; 
};

//fileUnitManager
//"controls the entire image"
/*
*	imgName 	- name of src image file
* 	rb 			- struct rootBlock for image file	
*
* 	avail_fObjs - (length of queue)number of fileObjects available for assignment
* 	max_fObjs 	- maximum number of fileObjects initialized
*	arr_fObjs 	- array of fileObject pointers
* 	fObjQ_head 	- head of fileObjectQueue
* 	fObjQ_tail 	- tail of fileObjectQueue
*
*	avail_fBlks - (length of queue) num fileBlocks available for assignment
*	max_fBlks 	- number of fileBlocks initialized
* 	user_fBlks 	- maximum number of fileBlocks available for asignment
*	arr_fBlks 	- array of fileBlock pointers
* 	fBlkQ_head 	- head of fileBlockQueue
* 	fBlkQ_tail 	- tail of fileBlockQueue  
*/
struct fileUnitManager{
	char* imgName;
	rBlk* rb;

	int avail_fObjs;
	int max_fObjs;
	fObj** arr_fObjs;
	fObj* fObjQ_head;
	fObj* fObjQ_tail;

	int avail_fBlks;
	int max_fBlks;
	int user_fBlks;
	fBlk** arr_fBlks;
	fBlk* fBlkQ_head;
	fBlk* fBlkQ_tail;
};

//operations
//init
void PushBlk(fBlk** headRef,int blockID);
void Push(fObj** headRef,int inode);

/*fileObject*/
void* initfObj(int ID);
void destroyfObj(fObj* fo);

/*fileBlock*/
void* initfBlk(int ID);
void destroyfBlk(fBlk* fo);

/*directoryFileEntry*/
void* initdFEnt(unsigned char type,char* name,unsigned short size,unsigned short hdr_offt);

/*FileUnitManager*/
void initfObjArrQ(fUMan* fm, fObj*** listRef,rBlk* rb,FILE* fptr);
void initfBlkArrQ(fUMan* fm, fBlk*** listRef,rBlk* rb,FILE* fptr);
void* initfUMan(rBlk* rb,FILE* fptr);
void destroyfUMan(fUMan* fm);


/*global init for fileUnitManager*/
void assemblefObjfBlks(fUMan* fm,int id);
void assembleActiveFiles(fUMan* fm);
void assembleGlobal(fUMan** globalRef, char* filename,FILE* fptr);

/*logical on/off of file object*/
int activatefObj(fUMan* fm,dFEnt* dfe);
void deactivatefObj(fUMan* fm, int id);

/*dequeue(connect)or enqueue(disconnect) fObj's fBlks*/ 
int connectfObjfBlks(fBlk* fb,void** endRef,fUMan* fm,int size,int id);
void disconnectfObjfBlks(fUMan* fm, int id);

/*extend or contract a file object*/
int extendfObj(fUMan* fm, unsigned short newlen, int id);
int reducefObj(fUMan* fm,unsigned short size,int id);

/*locate the starting block for read/write operation*/
int seekStartfBlk(fObj* fo, fBlk** hRef, size_t size, off_t off);

/*read/write on file id, seek to off_t and read size_t bytes to/from buffer*/
unsigned long readFromFile(fUMan* fm, int id,unsigned char* buffer, size_t size, off_t off);
unsigned long writeToFile(fUMan* fm,int id,const char* buffer,size_t size,off_t off);
void setTime(fUMan* fm);
//write fUMan* to disk
void writeToImage(fUMan* fm);

/*display functions*/
void displayrBlk(rBlk* rb);
void displayfBlk(fBlk* fb);
void displayfObj(fObj* fo);
void displaydFEnt(dFEnt* dfe);
void displayfUManStats(fUMan* fm);
void displayfBlkQ(fUMan* fm);
void displayfObjQ(fUMan* fm);
void displayActiveFiles(fUMan* fm);
void displayfUMan(fUMan* fm);
