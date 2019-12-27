#include "libfuman.h"

//display file object
void displayfObj(fObj* fo){
	printf("\nfObj_id:\t%d\t",fo->fObj_id);
	if (fo->active==1){
		//printf("%s\n",fo->name);
		printf("ACTIVE\n");
		printf("LEN:%d\n",fo->len);
		displaydFEnt(fo->dfe);
		fBlk* head=(fBlk*)(fo->HT_PN[0]);
		fBlk* tail=(fBlk*)(fo->HT_PN[1]);
		printf("-----------fBlks-------------\n");
		printf("(HEAD)");
		while (head!=tail){
			printf("[%d]->",head->fBlk_id);
			head=*(head->next);
		}
		printf("[%d](TAIL)\n",tail->fBlk_id);
	}else
		printf("%s\n",fo->active==-1 ? "RESERVED" : "INACTIVE");
}

void displayfBlk(fBlk* fb){
	printf("fBlk_id:\t%d\t",fb->fBlk_id);
	printf("%s\t",fb->active ? "ACTIVE":"INACTIVE");
	if (fb->active)
		printf("fObj_id:\t%d",fb->fObj_id);
	if (fb->data)
		printf("\tCONTAINS DATA.");
	printf("\n");
}

//display directory file entry
void displaydFEnt(dFEnt* dfe){
	unsigned char pad[MAX_LEN_FILENAME+1];
	memcpy(&pad,&(dfe->file_name),MAX_LEN_FILENAME);
	pad[MAX_LEN_FILENAME]='\0';
	printf("---dFEnt---\n");
	printf("\tfile_name:\t%s\n",pad);
	printf("\tType: %c\n\tcp_protect: %c\n",dfe->type,dfe->cp_protect);
	printf("\tstart_blk:\t%.04x(%d)\n",dfe->start_blk,dfe->start_blk);
	printf("\tfile_sz:\t%.04x(%d)\n",dfe->file_sz,dfe->file_sz);
	printf("\tBCD:   \t");
	for (int a=0; a< 8; a++)
		printf("%.02x ", dfe->BCD[a]);
	printf("\n\thdr_offt:\t%.04x(%d)\n",dfe->hdr_offt,dfe->hdr_offt);
}
//display root block
void displayrBlk(rBlk* rb){
	int a;
	printf("File Sig:\t");
	for (a=0; a< 16; a++)
		printf("%.02x ", rb->signature[a]);
	printf("\ncolorval:\t");
	for (a=0; a< 5; a++)
		printf("%.02x ", rb->color[a]);
	printf("\nblankreg:\t");
	printf("\ntimestamp:\t");
	for (a=0; a< 8; a++)
		printf("%.02x ", rb->BCD[a]);
	printf("\nblankreg2\t");
	printf("\nfat_start:\t%.04x(%u)", rb->fat_start, rb->fat_start);
	printf("\nfat_size:\t%u", rb->fat_sz);
	printf("\nlDirblk:\t%.04x(%u)", rb->end_dir_blk, rb->end_dir_blk);
	printf("\ndir_size:\t%u", rb->dir_sz);
	printf("\nfs_icon:\t%.04x",rb->icon);
	printf("\nuser_blocks:\t%u\n", rb->num_user_blks);
}
void displayfUManStats(fUMan* fm){
	displayfObjQ(fm);
	displayfBlkQ(fm);
	printf("%s STATS\n",fm->imgName);
	printf("\tfm->avail_fObjs: %d\n",fm->avail_fObjs);
	printf("\tfm->max_fObjs: %d\n",fm->max_fObjs);
	printf("\tfm->avail_fBlks: %d\n",fm->avail_fBlks);
	printf("\tfm->max_fBlks: %d\n",fm->max_fBlks);
	printf("\tfm->user_fblks: %d\n",fm->user_fBlks);
}
//display fileUnitManager
void displayfUMan(fUMan* fm){
	//dump_all
	printf("fm->avail_fObjs: %d\nfm->avail_fBlks: %d\n",fm->avail_fObjs,fm->avail_fBlks);
	for(int i=0;i<fm->max_fObjs;i++)
		displayfObj(fm->arr_fObjs[i]);
	for(int i=0;i<fm->max_fBlks;i++)
		displayfBlk(fm->arr_fBlks[i]);
}

void displayfObjQ(fUMan* fm){
	printf("Traversing fObj Queue\n");
	fObj* temp=fm->fObjQ_head;
	if (temp){
		printf("(HEAD)");
		while (temp->HT_PN[1]){
			printf("%d->",temp->fObj_id);
			temp=(fObj*)(temp->HT_PN[1]);
		}
		printf("%d(TAIL)\n",temp->fObj_id);
	}else{
		printf("NULL HEAD\n");
	}
	temp=fm->fObjQ_tail;
	if(temp){
		printf("(TAIL)");
		while(temp->HT_PN[0]){
			printf("%d->",temp->fObj_id);
			temp=(fObj*)(temp->HT_PN[0]);
		}
		printf("%d(HEAD)\n",temp->fObj_id);
	}else{
		printf("NULL TAIL\n");
	}
}

//iterate over file block queue
void displayfBlkQ(fUMan *fm){
	printf("Traversing fBlk Queue\n");
	fBlk* temp=fm->fBlkQ_head;
	if (temp){
		printf("(HEAD)");
		while(temp->next){
			printf("%d->",temp->fBlk_id);
			temp=*(temp->next);
		}
		printf("%d(TAIL)\n",temp->fBlk_id);
	}else{
		printf("NULL HEAD\n");
	}
	temp=fm->fBlkQ_tail;
	if (temp){
		printf("(TAIL)");
		while(temp->prev){
			printf("%d->",temp->fBlk_id);
			temp=*(temp->prev);
		}
		printf("%d(HEAD)\n",temp->fBlk_id);
	}else{
		printf("NULL TAIL\n");
	}
}

//display all active fileObjects
void displayActiveFiles(fUMan* fm){
	for(int i=0;i<fm->max_fObjs;i++){
		if (fm->arr_fObjs[i]->active==1)
			displayfObj(fm->arr_fObjs[i]);
	}
}