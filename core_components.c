#include "components.h"
//push a new fileObject onto the queue
void Push(fObj** headRef,int inode){
	fObj* nF=initfObj(inode);
	nF->HT_PN[1]=(void*)(*headRef);
	(*headRef)=nF;
}
//push a new fileBlock onto the queue
void PushBlk(fBlk** headRef,int blockID){
	fBlk* fB=initfBlk(blockID);
	(*headRef)=fB;
}

//create a new fileObject
void* initfObj(int ID){
	fObj* nF=malloc(sizeof(fObj));
	nF->type='O';
	nF->active=0;
	
	nF->dfe=NULL;
	nF->fObj_id=ID;
	nF->len=0;
	nF->HT_PN[0]=(void*)NULL;
	nF->HT_PN[1]=(void*)NULL;
	return nF;
}
//destructor, fileObject
void destroyfObj(fObj* fo){
	if (fo->active){
		free(fo->dfe);
		fo->dfe=NULL;
	}
	fo->HT_PN[0]=NULL;
	fo->HT_PN[1]=NULL;
	free(fo);
	fo=NULL;
}

//create a new fileBlock
void* initfBlk(int ID){
	fBlk* nB=malloc(sizeof(fBlk));
	nB->type='B';
	nB->active=0;
	nB->fObj_id=-1;
	nB->fBlk_id=ID;
	nB->data=NULL;
	nB->next=NULL;
	nB->prev=NULL;
	return nB;
}

void destroyfBlk(fBlk* fB){
	fB->prev=NULL;
	fB->next=NULL;
	if (fB->data)
		free(fB->data);
	fB->data=NULL;
	free(fB);
	fB=NULL;
}

//create a new directoryFileEntry
void* initdFEnt(unsigned char type,char* name,unsigned short size,unsigned short hdr_offt){
	dFEnt* dfe=calloc(sizeof(dFEnt),1);
	dfe->type=type;
	//int cpb=strlen(name)<MAX_LEN_FILENAME ? strlen(name) : MAX_LEN_FILENAME;
	memcpy(&dfe->file_name,name,MAX_LEN_FILENAME);
	
	DEBUG?:printf("name: %s\ndfe->file_name:%s\n",name,dfe->file_name);
	
	time_t rawtime;
	struct tm * timeinfo;
	time(&rawtime);
	timeinfo = localtime(&rawtime);
	unsigned int timeint[8]= {(timeinfo->tm_year+1900)/100,(timeinfo->tm_year+1900)%100, timeinfo->tm_mon+1, timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec, (timeinfo->tm_wday+6)%7};
	for (int i=0; i<8; i++)
		dfe->BCD[i]= ((timeint[i]/10) << 4) + (timeint[i]%10);
	
	dfe->file_sz=size;
	dfe->hdr_offt=hdr_offt;
	return dfe;
}
//create a new fileUnitManager

//init fObj array
void initfObjArrQ(fUMan* fm, fObj*** listRef,rBlk* rb,FILE* fptr){
	DEBUG?:printf("initfObjArrQ\n");
	fObj** ll=malloc(sizeof(fObj*)*fm->max_fObjs);
	//initialize reserved inode 0
	ll[0]=initfObj(0);
	ll[0]->active=-1;
	dFEnt* dfe0=calloc(sizeof(dFEnt),1);
	ll[0]->dfe=dfe0;
	//initialize reserved inode 1
	ll[1]=initfObj(1);
	ll[1]->active=-1;
	dFEnt* dfe1=calloc(sizeof(dFEnt),1);
	ll[1]->dfe=dfe1;
	
	fObj* nxt=NULL;
	//parse directory
	long dir_start=(rb->end_dir_blk+1-rb->dir_sz)*SZ_BLOCK;
	long dir_end=(rb->end_dir_blk+1)*SZ_BLOCK;
	int inodeVal=INODE_START;
	unsigned char buffer[SZ_DIRENTRY];
	memset(buffer,0,sizeof(unsigned char)*SZ_DIRENTRY);
	fseek(fptr,dir_start,SEEK_SET);
	//iterate over the entire directory section of image file
	/*
	* first two bytes of directory entry
	* will never be equal in an active file.
	*
	* if (ACTIVE Directory Entry)
	* 	add an active fileObject on the table
	*	decrement avail_fObjs
	* else
	* 	enqueue an INACTIVE fileObject by setting its prev/next ptrs
	*	add the inactive fileOBject to the table
	*/
	size_t bytes;
	while (ftell(fptr)!=dir_end){
		//fread(buffer,SZ_DIRENTRY,1,fptr);
		bytes=fread(buffer,sizeof(unsigned char),SZ_DIRENTRY,fptr);
		DEBUG?:printf("fObj fread:\t%d:\t%lu/%d retrieved\n",inodeVal,bytes,SZ_DIRENTRY);
		if (buffer[0]!=buffer[1]){
			dFEnt* dfe=malloc(sizeof(dFEnt));
			memcpy(&*dfe,&buffer,SZ_DIRENTRY);
			
			fObj* fo=initfObj(inodeVal);
			fo->active=1;
			fo->dfe=dfe;
			ll[inodeVal]=fo;
			fm->avail_fObjs--;
		}else{
			Push(&(fm->fObjQ_head),inodeVal);
			ll[inodeVal]=fm->fObjQ_head;
			if ((nxt=(fObj*)ll[inodeVal]->HT_PN[1]) && !nxt->HT_PN[0])
				nxt->HT_PN[0]=(void*)ll[inodeVal];
		}
		inodeVal++;
	}
	
	//set tail pointer to back of queue;
	fm->fObjQ_tail=fm->fObjQ_head;
	while(fm->fObjQ_tail->HT_PN[1]){
		fm->fObjQ_tail=(fObj*)(fm->fObjQ_tail->HT_PN[1]);
	}
	
	*(listRef)=ll;
}

//init fBlk array
void initfBlkArrQ(fUMan* fm, fBlk*** listRef,rBlk* rb,FILE* fptr){
	DEBUG?:printf("initfBlkArrQ\n");
	
	fBlk** nextBlk=NULL;
	fBlk** lb=malloc(sizeof(fBlk*)*fm->max_fBlks);
	for(int i=0;i<fm->max_fBlks;i++)
		lb[i]=NULL;
	
	long fat_start=(rb->fat_start*SZ_BLOCK);
	long fat_end=(long)(rb->fat_sz*SZ_BLOCK)+fat_start;
	int blockID=0;
	int hold=0;
	unsigned short bufferval=0;
	fseek(fptr,fat_start,SEEK_SET);
	//iterate over FAT
	/*	USER SECTION
	*	FAT composed of 2 byte entries. Each entry will be
	* 	less than fm->max_fBlks, identifing the following entry EXCEPT FOR
	*	(65530)0xFFFA : last block of file
	*	(65532)0xFFFC : not in use(enqueue)
	*	
	*	rootBlock defines the number of user blocks
	* 	fm->avail_fBlks<=fm->user_fBlks<=fm->max_fBlks
	* 	
	*/
	size_t bytes;
	while (blockID<fm->user_fBlks){
		//fread(&bufferval,sizeof(unsigned short),1,fptr);
		bytes=fread(&bufferval,sizeof(unsigned short),1,fptr);
		DEBUG?:printf("fBlk fread:\t%d:\t%lu/%d retrieved\n",blockID,bytes,1);

		if (bufferval==65532){
			hold= fm->fBlkQ_head ? fm->fBlkQ_head->fBlk_id : -1;
			PushBlk(&(fm->fBlkQ_head),blockID);
			lb[blockID]=fm->fBlkQ_head;
			lb[blockID]->next= (hold!=-1 ? &lb[hold] : NULL);
			if((nextBlk=lb[blockID]->next) && !(*nextBlk)->prev)
				(*nextBlk)->prev=&lb[blockID];
		}else{
			fBlk* fb=initfBlk(blockID);
			fb->active=1;
			if (bufferval!=65530)
				fb->next=&(lb[bufferval]);
			lb[blockID]=fb;
			fm->avail_fBlks--;
		}
		blockID++;
	}

	//set tail of queue  to back of queue
	if (fm->fBlkQ_head){
		fm->fBlkQ_tail=fm->fBlkQ_head;
		while (fm->fBlkQ_tail->next){
			fm->fBlkQ_tail=*(fm->fBlkQ_tail->next);
		}
	}
	/* 
	*	for accounting purposes, and scaling, fm->max_fBlks
	* 	are added to fm->arr_fBlks.
	*
	* 	However,only block 0 to fm->user_fBlks are available for use
	*
	* 	Any block after this point must not be a user block,
	* 	so do not add any to queue.
	* 	Note that chains may still exist.
	*/
	while (ftell(fptr)<fat_end){
		fread(&bufferval,sizeof(unsigned short),1,fptr);
		if (bufferval==65532){
			lb[blockID]=initfBlk(blockID);
		}else{
			fBlk* fb=initfBlk(blockID);
			fb->active=1;
			if (bufferval!=65530)
				fb->next=&(lb[bufferval]);
			lb[blockID]=fb;
		}
		blockID++;
	}
	(*listRef)=lb;
}

//init fileUnitManager
void* initfUMan(rBlk* rb,FILE* fptr){
	DEBUG?:printf("initfUMan\n");
	fUMan* fm=malloc(sizeof(fUMan));
	fm->imgName=NULL;
	fm->rb=NULL;
	
	fm->avail_fObjs=(rb->dir_sz*SZ_BLOCK)/SZ_DIRENTRY;
	fm->max_fObjs=fm->avail_fObjs+INODE_START;
	
	fm->arr_fObjs=NULL;
	fm->fObjQ_head=NULL;
	fm->fObjQ_tail=NULL;
	initfObjArrQ(fm,&(fm->arr_fObjs),rb,fptr);
	
	fm->avail_fBlks=rb->num_user_blks;
	fm->max_fBlks=(rb->fat_sz*SZ_BLOCK)/2;
	fm->user_fBlks=fm->avail_fBlks;
	fm->arr_fBlks=NULL;
	fm->fBlkQ_head=NULL;
	fm->fBlkQ_tail=NULL;
	initfBlkArrQ(fm,&(fm->arr_fBlks),rb,fptr);
	fm->rb=rb;
	return fm;
}

//destructor, fileUnitManager
void destroyfUMan(fUMan* fm){
	for(int i=0;i<fm->max_fObjs;i++){
		destroyfObj(fm->arr_fObjs[i]);
	}
	free(fm->arr_fObjs);
	for(int i=0;i<fm->max_fBlks;i++){
		destroyfBlk(fm->arr_fBlks[i]);
	}
	free(fm->rb);
	free(fm->arr_fBlks);
	free(fm->imgName);
	free(fm);
}


//finish building files present in image
void assemblefObjfBlks(fUMan* fm,int id){
	FILE* fptr;//spc
	fptr=fopen(fm->imgName,"rb");//spc
	fObj* fo=fm->arr_fObjs[id];
	fo->len=fo->dfe->file_sz*SZ_BLOCK;
	fo->HT_PN[0]=(void*)(fm->arr_fBlks[fo->dfe->start_blk]);
	fBlk* temp=(fBlk*)(fo->HT_PN[0]);
	size_t bytes;
	while(temp->next){
		temp->data=(unsigned char*)calloc(sizeof(unsigned char),SZ_BLOCK);//spc
		if (fseek(fptr, temp->fBlk_id*SZ_BLOCK,0)){//spc
			DEBUG?:printf("invalid fseek!\n");
			DEBUG?:displayfBlk(temp);
		}
		bytes=fread(temp->data,sizeof(unsigned char),SZ_BLOCK,fptr);//spc
		DEBUG?:printf("assemble fBObjfBlk fread:\t%d:\t%lu/%d retrieved\n",temp->fBlk_id,bytes,SZ_BLOCK);
		(*temp->next)->prev=&(fm->arr_fBlks[temp->fBlk_id]);
		//DEBUG?:printf("%d->",temp->fBlk_id);
		temp->fObj_id=id;
		temp=*(temp->next);
	}

	temp->data=(unsigned char*)calloc(sizeof(unsigned char),SZ_BLOCK);
	fseek(fptr,temp->fBlk_id*SZ_BLOCK,0);//spc
	bytes=fread(temp->data,sizeof(unsigned char),SZ_BLOCK,fptr);//spc
	DEBUG?:printf("assemble fBObjfBlk fread:\t%d:\t%lu/%d retrieved\n",temp->fBlk_id,bytes,SZ_BLOCK);
	//fread(temp->data,SZ_BLOCK,1,fptr);//spc
	fclose(fptr);//spc
	//DEBUG?:printf("%dEND\n",temp->fBlk_id);
	temp->fObj_id=id;
	fo->HT_PN[1]=(void*)(fm->arr_fBlks[temp->fBlk_id]);
}

//passthrough to assemble a fileOBject
void assembleActiveFiles(fUMan* fm){
	for(int i=0;i<fm->max_fObjs;i++){
		if (fm->arr_fObjs[i]->active==1)
			assemblefObjfBlks(fm,i);
	}
}

//build main fileUnitManager
void assembleGlobal(fUMan** gR,char* filename,FILE* fptr){
	fUMan* fm=NULL;
	rBlk* rb=calloc(sizeof(unsigned char),SZ_BLOCK);
	fread(rb,SZ_BLOCK,1,fptr);
	displayrBlk(rb);
	fm=initfUMan(rb,fptr);
	fclose(fptr);
	fm->imgName=calloc(sizeof(char),strlen(filename)+1);
	memcpy(fm->imgName,filename,strlen(filename));
	DEBUG?:printf("imgName: %s\n", fm->imgName);
	assembleActiveFiles(fm);
	(*gR)=fm;
}


//dequeue fileObject
//init fileBlocks=1
//toggle fileObject active=1
int activatefObj(fUMan* fm,dFEnt* dfe){
	displayfUManStats(fm);
	fObj* fo=fm->arr_fObjs[fm->fObjQ_head->fObj_id];
	fm->fObjQ_head=(fObj*)(fm->fObjQ_head->HT_PN[1]);
	fm->fObjQ_head->HT_PN[0]=(void*)NULL;

	fo->HT_PN[0]=(void*)NULL;
	fo->HT_PN[1]=(void*)NULL;
	fo->dfe=dfe;
	fo->active=1;
	fo->dfe->start_blk=fm->fBlkQ_head->fBlk_id;

	fo->HT_PN[0]=(void*)(fm->arr_fBlks[fm->fBlkQ_head->fBlk_id]);
	fBlk* fb=(fBlk*)(fo->HT_PN[0]);
	
	fb->active=1;
	fb->fObj_id=fo->fObj_id;
	
	fb->data=(unsigned char*)calloc(sizeof(unsigned char),SZ_BLOCK);
	if (fb->next){
		(*fb->next)->prev=NULL;
		fm->fBlkQ_head=*fb->next;
		fb->next=NULL;
	}else{
		fm->fBlkQ_head=NULL;
		fm->fBlkQ_tail=NULL;
	}
	
	fo->HT_PN[1]=(void*)(fm->arr_fBlks[fb->fBlk_id]);
	fm->avail_fBlks--;
	fm->avail_fObjs--;
	return fo->fObj_id;
}

//enqueue fileObject
//disconnect fileObjectBlocks;
void deactivatefObj(fUMan* fm,int id){
	disconnectfObjfBlks(fm,id);
	fObj* fo=fm->arr_fObjs[id];
	fo->len=0;
	free(fo->dfe);
	fo->dfe=NULL;
	fo->active=0;
	fm->fObjQ_tail->HT_PN[1]=(void*)(fm->arr_fObjs[id]);
	fo->HT_PN[0]=(void*)(fm->arr_fObjs[fm->fObjQ_tail->fObj_id]);
	fm->fObjQ_tail=(fObj*)(fm->fObjQ_tail->HT_PN[1]);
	fm->avail_fObjs++;
}


//extend fileObject id's existing chain of fileBlocks by size
int connectfObjfBlks(fBlk* fb,void** endRef,fUMan* fm,int size,int id){
	DEBUG?:printf("\nextending\n(HEAD)");
	fBlk* fh=fb;
	int blockCounter=1;
	while(blockCounter<size){
		DEBUG?:printf("%d->",fh->fBlk_id);
		fh->data=calloc(sizeof(unsigned char),SZ_BLOCK);
		fh->fObj_id=id;
		fh->active=1;
		if (fh->next){
			(*fh->next)->prev=&(fm->arr_fBlks[fh->fBlk_id]);
			fh=*(fh->next);
		}else{
			DEBUG?:printf("NULL POINTER IS NEXT\n");
			break;
		}
		blockCounter++;
	}
	DEBUG?:printf("%d(END)\n",fh->fBlk_id);
	fh->data=calloc(sizeof(unsigned char),SZ_BLOCK);
	fh->active=1;
	fh->fObj_id=id;
	if (fh->next){
		(*fh->next)->prev=NULL;
		fm->fBlkQ_head=*(fh->next);
	}else{
		DEBUG?:printf("fBlkQ is EMPTY\n");
		fm->fBlkQ_head=NULL;
		fm->fBlkQ_tail=NULL;
	}
	DEBUG?:printf("added %d blocks\n",blockCounter);
	fh->next=NULL;
	(*endRef)=(void*)(fm->arr_fBlks[fh->fBlk_id]);
	return blockCounter;
}

//clear fileObject id's fileBlocks 
void disconnectfObjfBlks(fUMan* fm, int id){
	fObj* fo=fm->arr_fObjs[id];
	fBlk* head=(fBlk*)(fo->HT_PN[0]);
	fBlk* tail=(fBlk*)(fo->HT_PN[1]);
	if (!fm->fBlkQ_tail)
		DEBUG?:printf("tail is null\n");
	else{
		DEBUG?:printf("tail is not null?\n");
		DEBUG?:displayfBlk(fm->fBlkQ_tail);
	}
	if (!fm->fBlkQ_head){
		DEBUG?:printf("Setting NULL head\n");
		fm->fBlkQ_head=fm->arr_fBlks[head->fBlk_id];
	}else{
		fm->fBlkQ_tail->next=&(fm->arr_fBlks[head->fBlk_id]);
		head->prev=&(fm->arr_fBlks[fm->fBlkQ_tail->fBlk_id]);
		(*head->prev)->next=&(fm->arr_fBlks[head->fBlk_id]);
	}
	while(head!=tail){
		DEBUG?:printf("%d->",head->fBlk_id);
		free(head->data);
		head->data=NULL;
		head->fObj_id=-1;
		head->active=0;
		head=(*head->next);
	}
	free(tail->data);
	tail->data=NULL;
	tail->fObj_id=-1;
	tail->active=0;
	tail->next=NULL;
	fm->fBlkQ_tail=tail;
	fm->avail_fBlks+=fo->dfe->file_sz;
	fo->dfe->file_sz=0;
	fo->HT_PN[0]=(void*)NULL;
	fo->HT_PN[1]=(void*)NULL;
}

//extend fileObject by newBlocks
int extendfObj(fUMan* fm,unsigned short newBlocks,int id){
	fObj* fo=fm->arr_fObjs[id];
	fBlk* fb=(fBlk*)(fo->HT_PN[1]);
	fb->next=&(fm->arr_fBlks[fm->fBlkQ_head->fBlk_id]);
	(*fb->next)->prev=&(fm->arr_fBlks[fb->fBlk_id]);
	fb=*(fb->next);
	int added=0;
	added=connectfObjfBlks(fb,&(fo->HT_PN[1]),fm,newBlocks,id);
	fm->avail_fBlks-=added;
	fo->dfe->file_sz+=added;
	return added;
}

//reduce fileObject to newLen
/*
* 	count newLen nodes from head, set relative tail
*	relative tail->next to absolute tail gets reassigned to dump node
* 	disconnect the dump node's fBlks 
*/
int reducefObj(fUMan* fm, unsigned short newLen, int id){
	fObj* fo=fm->arr_fObjs[id];
	fBlk* head=(fBlk*)(fo->HT_PN[0]);
	int count=1;
	while (count<newLen){
		head=*head->next;
		count++;
	}
	fBlk* tail=(fBlk*)(fm->arr_fBlks[head->fBlk_id]);
	(*tail->next)->prev=NULL;
	
	fObj* dump=fm->arr_fObjs[0];
	dump->HT_PN[0]=(void*)(*tail->next);
	dump->HT_PN[1]=fo->HT_PN[1];
	
	tail->next=NULL;
	
	fo->HT_PN[1]=(void*)(fm->arr_fBlks[tail->fBlk_id]);
	
	dump->dfe->file_sz=fo->dfe->file_sz-newLen;
	disconnectfObjfBlks(fm,0);
	
	fo->dfe->file_sz=newLen;
	return newLen;
}


int seekStartfBlk(fObj* fo, fBlk** headRef, size_t size, off_t off){
	int startBlock=floor(off/SZ_BLOCK);
	DEBUG?:printf("fObj%d\t",fo->fObj_id);
	DEBUG?:printf("seekStartBlock%d\n",startBlock);
	fBlk* head=(fBlk*)(fo->HT_PN[0]);
	fBlk* tail=(fBlk*)(fo->HT_PN[1]);
	DEBUG?:printf("[HEAD]");
	int i=0;
	while(head!=tail && i<startBlock){
	//for(int i=0;i<startBlock;i++){
		DEBUG?:printf("%d-",head->fBlk_id);
		head=*head->next;
		i++;
	}
	DEBUG?:printf("(START)");
	DEBUG?:printf("\nstartBlock: %d\ni: %d\n",startBlock,i);
	(*headRef)=head;
	if (i<startBlock)
		return SZ_BLOCK;
	return 0;
}


unsigned long readFromFile(fUMan* fm, int id,unsigned char* buf, size_t size, off_t off){
	fObj* fo=fm->arr_fObjs[id];
	DEBUG?:printf("\tReading bytes from file...\n");
	DEBUG?:printf("buf addr:\t%p\n",&(*buf));
	DEBUG?:printf("READ\n\tsource buf sz %lu,size %lu\n",sizeof(buf),size);
	DEBUG?:printf("\treading ino: %d,size: %lu, off_t: %lu\n",id,size,off);
	fBlk* head=NULL;
	int adj=seekStartfBlk(fo,&head,size,off);
	DEBUG?:printf("READ adj: %d\n",adj);
	if (adj)
		return 0;
	unsigned char* wp=head->data+(off%512);
	unsigned char* total=buf;
	while (head->next && (total-buf)+(SZ_BLOCK-(wp-head->data))<size){
		DEBUG?:printf("[%d]\t%lu/%lu bytes read.(%lu remaining)\n",head->fBlk_id,total-buf,size,(buf+size)-total);
		memcpy(total,wp,SZ_BLOCK-(wp-head->data));
		//DEBUG?:printf("(read)[%d]->",head->fBlk_id);
		total+=SZ_BLOCK-(wp-head->data);
		head=*head->next;
		wp=head->data;
	}

	DEBUG?:printf("[%d]\t%lu/%lu bytes read.(%lu remaining)\n",head->fBlk_id,total-buf,size,(buf+size)-total);
	size_t last=(total+SZ_BLOCK)>(buf+size) ? size-(total-buf) : SZ_BLOCK;
	//size_t last=(size-(total-buf))>SZ_BLOCK ? SZ_BLOCK : size-(total-buf);
	last-=(wp-head->data);
	DEBUG?:printf("about to write %lu bytes.\n",last);
	memcpy(total,wp,last);
	total+=last;
	if (total-buf<size)
		DEBUG?:printf("WARNING (File is short,EARLY TAIL)");
	else
		DEBUG?:printf("(TAIL)\n");
	DEBUG?:printf("max buf size: %lu\tbytes read to buf: %lu\n",size,total-buf);
	return total-buf;
}


unsigned long writeToFile(fUMan* fm,int id,const char* buf,size_t size,off_t off){

	int blocksToWrite=ceil(size/SZ_BLOCK);
	fObj* fo=fm->arr_fObjs[id];
	
	DEBUG?:printf("buf addr:\t%p\n",&(*buf));
	DEBUG?:printf("PLACE\n\tsource buf sz %lu,size %lu\n",sizeof(buf),size);
	DEBUG?:printf("\twriting ino: %d,size: %lu, off_t: %li\n",id,size,off);
	DEBUG?:printf("\tdest buf sz %li\n",(fo->dfe->file_sz*SZ_BLOCK)-off);
	DEBUG?:printf("\tBlocks to write:%d\n",blocksToWrite);
	
	fBlk* head=NULL;
	int adj=seekStartfBlk(fo,&head,size,off);
	DEBUG?:printf("WRITE adj: %d\n",adj);
	if (adj)
		return 0;
	unsigned char* wp=head->data+(off%512);
	
	const char* total=buf;
	while (head->next && (total-buf)+(SZ_BLOCK-(wp-head->data))<size){
		DEBUG?:printf("[%d]\t%lu/%lu bytes written.(%lu remaining)\n",head->fBlk_id,total-buf,size,(buf+size)-total);
		memcpy(wp,total,SZ_BLOCK-(wp-head->data));
		//DEBUG?:printf("(write)[%d]->",head->fBlk_id);
		total+=SZ_BLOCK-(wp-head->data);
		head=*head->next;
		wp=head->data;
	}
	DEBUG?:printf("[%d]\t%lu/%lu bytes written.(%lu remaining)\n",head->fBlk_id,total-buf,size,(buf+size)-total);

	//how much is left to write?
	size_t last= (size-(total-buf))>SZ_BLOCK ? SZ_BLOCK : size-(total-buf);
	last-=(wp-head->data);
	//size_t last= (total+SZ_BLOCK)>(buf+size) ? (buf+size)-(total) : SZ_BLOCK;
	//last=total-buf+SZ_BLOCK<size
	DEBUG?:printf("about to write %lu bytes.\n",last);
	memcpy(wp,total,last);
	memset(wp+last, 0, SZ_BLOCK-last);
	total+=last;
	if (total-buf<size)
		DEBUG?:printf("WARNING,EARLY TAIL\n");
	else
		DEBUG?:printf("(TAIL)\n");
	DEBUG?:printf("max buf size: %lu\tbytes written to buf: %lu\n",size,total-buf);
	fo->len=off+(total-buf);
	return total-buf;
}

//save image to disk
void writeToImage(fUMan* fm){
	int fat_len=(fm->rb->fat_sz*SZ_BLOCK)/2;
	unsigned short FAT_TABLE[fat_len];
	memset(FAT_TABLE,0,sizeof(unsigned short)*fat_len);
	for(int i=0;i<fat_len;i++)
		FAT_TABLE[i]=65532;
	
	dFEnt DIR_TABLE[fm->max_fObjs-2];
	memset(DIR_TABLE,0,sizeof(dFEnt)*(fm->max_fObjs-2));
	FILE* fptr;
	fptr=fopen(fm->imgName,"r+");
	//unsigned long DIR_START=dir_end-
	long dir_start=(fm->rb->end_dir_blk+1-fm->rb->dir_sz)*SZ_BLOCK;
	long dir_end=(fm->rb->end_dir_blk+1)*SZ_BLOCK;
	for(int i=INODE_START;i<fm->max_fObjs;i++){
		if (fm->arr_fObjs[i]->active==1){
			DEBUG?:printf("here;DIR_TABLE %d\n",i-INODE_START);
			memcpy(DIR_TABLE+(i-INODE_START),fm->arr_fObjs[i]->dfe,sizeof(dFEnt));
		}
	}
	for(int i=0;i<fm->max_fBlks;i++){
		if (fm->arr_fBlks[i]->active){
			if (fm->arr_fBlks[i]->next){
				unsigned short adj=(*fm->arr_fBlks[i]->next)->fBlk_id;
				FAT_TABLE[i]=adj;
			}else{
				FAT_TABLE[i]=65530;
			}
		}else{
			FAT_TABLE[i]=65532;
		}
	}
	for(int i=0;i<fat_len;i++){
		if (!(i%8))
			DEBUG?:printf("\n");
		DEBUG?:printf("%.04x ",FAT_TABLE[i]);
	}
	unsigned char zb[SZ_BLOCK];
	memset(zb,0,SZ_BLOCK);
	fseek(fptr,0,SEEK_SET);
	for (int i=0;i<fm->user_fBlks;i++){
		if (fm->arr_fBlks[i]->active==1){
			fwrite(fm->arr_fBlks[i]->data,SZ_BLOCK,1,fptr);
		}
		else{
			fwrite(zb,SZ_BLOCK,1,fptr);
		}
	}
	fseek(fptr,dir_start,SEEK_SET);
	fwrite(DIR_TABLE,sizeof(dFEnt),fm->max_fObjs-2,fptr);
	long fat_start=(fm->rb->fat_start*SZ_BLOCK);
	fseek(fptr,fat_start,SEEK_SET);
	fwrite(FAT_TABLE,sizeof(FAT_TABLE),1,fptr);
	fseek(fptr,-SZ_BLOCK,SEEK_END);
	fwrite(fm->rb,SZ_BLOCK,1,fptr);
	fclose(fptr);
}