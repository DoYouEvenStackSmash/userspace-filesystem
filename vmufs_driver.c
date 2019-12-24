
// #define _GNU_SOURCE
// #define FUSE_USE_VERSION 26
// #include <fuse.h>
// #include <time.h>
// #include <fuse_lowlevel.h>
// #include <fuse_common.h>
#include "components.h"
// #include "fuse_fxns.h"
//#define
static fUMan* globalfUM=NULL;



#define min(x, y) ((x) < (y) ? (x) : (y))

int lookupName(const char *name){
	char buf[13]={[0 ... 12]=0};
	memcpy(buf,name,min(strlen(name),12));
	DEBUG?:printf("looking up %s\n", buf);
	int ret=-1;
	for(int i=INODE_START;i<globalfUM->max_fObjs;i++){
		if (globalfUM->arr_fObjs[i]->active==1){
			if (memcmp(buf,globalfUM->arr_fObjs[i]->dfe->file_name,12)==0){
				ret=i;
				return ret;
			}
		}
	}
	return -1;
}
static int reply_buf_limited(fuse_req_t req, const char *buf, size_t bufsize,
	off_t off, size_t maxsize)
{
	DEBUG?:printf("REPLY BUF LIMITED:\n\tbufsize: %lu, off: %lu, maxsize: %lu\n", bufsize,off,maxsize);
	DEBUG?:printf("\tlimited buf addr: %p\n",&(*buf));
	//DEBUG?:printf("\n\n***********BUFFER SIZE***********\nreply_buf_limited\n\nbuf:%lu *buf: %lu &buf:%lu\n*************************\n",sizeof(buf),sizeof(*buf),sizeof(&buf));
	//return fuse_reply_buf(req,buf,bufsize);
	if (off < bufsize){
		return fuse_reply_buf(req, buf + off,min(bufsize - off, maxsize));
	// }
	// else if (bufsize==maxsize){
	// 	printf("ideal?\n")
	// 	return fuse_reply_buf(req,buf,maxsize);
	}else{
		DEBUG?:printf("fragging\n");
		return fuse_reply_buf(req, NULL, 0);
	}
}

static int vmufs_stat(fuse_ino_t ino, struct stat *stbuf)
{
	if (ino<1 || globalfUM->arr_fObjs[ino]->active==0)
		return -1;
	stbuf->st_ino=ino;
	unsigned int unbcd[8] ={[0 ... 7] =0};
	int i;
	for (i=0; i<8; i++)
		unbcd[i] =globalfUM->arr_fObjs[ino]->dfe->BCD[i] - 6 * (globalfUM->arr_fObjs[ino]->dfe->BCD[i]>>4);
	struct tm timeinfo;
	timeinfo.tm_year = (unbcd[0] * 100 +unbcd[1])-1900;
	timeinfo.tm_mon = unbcd[2]-1;
	timeinfo.tm_mday = unbcd[3];
	timeinfo.tm_hour = unbcd[4];
	timeinfo.tm_min = unbcd[5];
	timeinfo.tm_sec = unbcd[6];
	timeinfo.tm_isdst=0;
	stbuf->st_mtime = mktime(&timeinfo);
	if (ino==1){
		//stbuf->st_blocks=1;
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		stbuf->st_blksize=SZ_BLOCK;
		//stbuf->st_size = globalfUM->arr_fObjs[ino]->dfe->file_sz*SZ_BLOCK;
		//stbuf->st_uid=fuse_get_context()->uid;
		//stbuf->st_gid=fuse_get_context()->gid;
		//stbuf->st_blocks = globalfUM->arr_fObjs[ino]->dfe->file_sz;
		return 0;
	}
	//globalFUM->arr_fObjs[ino]->dfe->BCD[i]
	stbuf->st_mode=S_IFREG | 0644;
	stbuf->st_blksize=SZ_BLOCK;
	stbuf->st_size = globalfUM->arr_fObjs[ino]->len;//->dfe->file_sz*SZ_BLOCK;//globalfUM->arr_fObjs[ino]->len;
	stbuf->st_uid=fuse_get_context()->uid;
	stbuf->st_gid=fuse_get_context()->gid;
	stbuf->st_blocks = globalfUM->arr_fObjs[ino]->dfe->file_sz;
	return 0;
}


static void vmufs_getattr(fuse_req_t req, fuse_ino_t ino,struct fuse_file_info *fi){
	struct stat stbuf;
	(void) fi;
	memset(&stbuf,0,sizeof(stbuf));
	if (vmufs_stat(ino,&stbuf)==-1)
		fuse_reply_err(req,ENOENT);
	else
		fuse_reply_attr(req,&stbuf,100);
}

static void vmufs_lookup(fuse_req_t req,fuse_ino_t parent, const char *name){
	int flag=0;
	struct fuse_entry_param e;
	memset(&e,0,sizeof(e));
	char buf[13]={[0 ... 12]=0};
	memcpy(buf,name,min(strlen(name),12));
	DEBUG?:printf("looking up %s\n", buf);
	for(int i=0;i<globalfUM->max_fObjs;i++){
		if (globalfUM->arr_fObjs[i]->active!=0){
			if (memcmp(buf,globalfUM->arr_fObjs[i]->dfe->file_name,12)==0){
				e.ino=i;
				e.attr_timeout=100;
				e.entry_timeout=100;
				vmufs_stat(e.ino,&e.attr);
				flag=1;
				break;
				//break;
			}
		}
	}
	if (!flag)
		fuse_reply_err(req,ENOENT);
	else
		fuse_reply_entry(req,&e);
}

static void vmufs_open(fuse_req_t req,fuse_ino_t ino,struct fuse_file_info *fi){
	(void) fi;
	DEBUG?:printf("\nopen?\n");
	if (ino < 2)
		fuse_reply_err(req,EISDIR);
	else 
		fuse_reply_open(req,fi);
		//int fd=open(globalfUM->arr_fObjs[ino]->dfe->file_name,O_RDONLY)_
	
}

struct dirbuf{
	char* p;
	size_t size;
};

static void dirbuf_add(fuse_req_t req,struct dirbuf* b,const char* name,fuse_ino_t ino){
	struct stat stbuf;
	memset(&stbuf, 0, sizeof(stbuf));
	
	size_t old_size=b->size;
	char buf[13]={[0 ... 12]=0};
	memcpy(buf,name,min(strlen(name),12));
	b->size+=fuse_add_direntry(req,NULL,0,buf,NULL,0);
	b->p=realloc(b->p,b->size);
	stbuf.st_ino = ino;
	vmufs_stat(ino,&stbuf);
	fuse_add_direntry(req, b->p + old_size, b->size - old_size, buf, &stbuf,b->size);
}


static void vmufs_read(fuse_req_t req, fuse_ino_t ino, size_t size,
	off_t off, struct fuse_file_info *fi){
	(void) fi;
	if (ino<2)
		fuse_reply_err(req,EISDIR);
	else{
		unsigned char buf[size];
		memset(buf,0,size*sizeof(unsigned char));
		DEBUG?:printf("buf addr: %p\n",buf);
		DEBUG?:printf("buf size: %lu\n",sizeof(buf));
		DEBUG?:printf("READ\n\tino: %lu,size: %lu, off_t: %lu\n",ino,size,off);
		size_t bytesRead;
		bytesRead=readFromFile(globalfUM,ino,buf,size,off);
		DEBUG?:printf("\tREAD: providing read size %lu\n",bytesRead);
		//reply_buf_limited(req,buf,bytesRead,off,size);
		if (bytesRead)
			fuse_reply_buf(req,buf,bytesRead);
		else
			fuse_reply_buf(req,NULL,0);
		
		//DEBUG?:printf("READ CALLBACK\n\tcallback buf has len %lu\n",sizeof(buf));

	}
	//if (strcmp(path+1,))
}

static void vmufs_readdir(fuse_req_t req, fuse_ino_t ino,size_t size,off_t off,struct fuse_file_info *fi){
	(void) fi;
	//printf("seeking ino %d\n",ino);
	//displayActiveFiles(globalfUM);
	if (ino!=1)
		fuse_reply_err(req,ENOTDIR);
	else{
		struct dirbuf b;
		memset(&b, 0, sizeof(b));
		dirbuf_add(req, &b, ".", 1);
		dirbuf_add(req, &b, "..", 1);
		//unsigned int index;
		for(int i=INODE_START;i<globalfUM->max_fObjs;i++){
			if (globalfUM->arr_fObjs[i]->active==1)
				dirbuf_add(req,&b,globalfUM->arr_fObjs[i]->dfe->file_name,i);
		}
		// displayActiveFiles(globalfUM);
		reply_buf_limited(req, b.p, b.size, off, size);
		free(b.p);

	}
	//DEBUG?:displayfUManStats(globalfUM);

}

static void vmufs_setattr(fuse_req_t req, fuse_ino_t ino, struct stat *attr,
	int to_set, struct fuse_file_info *fi){
	(void) fi;
	if (ino<1){

		DEBUG?:printf("what the fuck man\n");
		fuse_reply_err(req, 0);
	}else{
		unsigned int unbcd[8] ={[0 ... 7] =0};
		int i;
		for (i=0; i<8; i++){
			unbcd[i] =globalfUM->arr_fObjs[ino]->dfe->BCD[i] - 6 * (globalfUM->arr_fObjs[ino]->dfe->BCD[i]>>4);
		}
		struct tm timeinfo;
		timeinfo.tm_year = (unbcd[0] * 100 +unbcd[1])-1900;
		timeinfo.tm_mon = unbcd[2]-1;
		timeinfo.tm_mday = unbcd[3];
		timeinfo.tm_hour = unbcd[4];
		timeinfo.tm_min = unbcd[5];
		timeinfo.tm_sec = unbcd[6];
		timeinfo.tm_isdst=0;
		attr->st_mtime = mktime(&timeinfo);
		attr->st_blksize=SZ_BLOCK;
		attr->st_ino = ino;
		attr->st_mode= ino==1 ? S_IFDIR |0755 : S_IFREG | 0644; 
		attr->st_nlink = ino==1 ? 2 : 1;
		attr->st_uid = fuse_get_context()->uid;
		attr->st_gid = fuse_get_context()->gid;
		attr->st_size=globalfUM->arr_fObjs[ino]->len;//->dfe->file_sz*SZ_BLOCK;
		attr->st_blocks = globalfUM->arr_fObjs[ino]->dfe->file_sz;
		fuse_reply_attr(req, attr, 100);
	}
}

static void vmufs_flush(fuse_req_t req, fuse_ino_t ino,struct fuse_file_info *fi){
	(void) fi;
	fuse_reply_err(req,ENOSYS);
}

static void vmufs_create(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode,struct fuse_file_info *fi){
	(void)fi;
	if (globalfUM->avail_fBlks==0 || globalfUM->avail_fObjs==0){
		fuse_reply_err(req,ENOSPC);
		//return;
	}else{
		int flag=0;
		DEBUG?:printf("looking up %s\n", name);
		char buf[13]={[0 ... 12]=0};
		memcpy(buf,name,min(strlen(name),12));
		for(int i=INODE_START;i<globalfUM->max_fObjs;i++){
			if (globalfUM->arr_fObjs[i]->active==1){
				if (memcmp(buf,globalfUM->arr_fObjs[i]->dfe->file_name,12)==0){
					//fuse_reply_err(req,EEXIST);
					flag=1;
					break;
				}
			}
		}
		if (!flag){
			DEBUG?:printf("creating %s\n",buf);
			struct fuse_entry_param e;
			memset(&e, 0, sizeof(e));
			e.ino=activatefObj(globalfUM,initdFEnt(0x33,buf,1,0));
			e.attr_timeout=100;
			e.entry_timeout=100;
			vmufs_stat(e.ino,&e.attr);
			//e.attr.st_size=0;
			//e.attr.st_mode=mode;
			fuse_reply_create(req,&e,fi);
		}else{
			fuse_reply_err(req,EEXIST);
		}
	}
} 


static void vmufs_mknod(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode,dev_t rdev){
	if (globalfUM->avail_fBlks==0 || globalfUM->avail_fObjs==0){
		fuse_reply_err(req,ENOSPC);
		//return;
	}else{
		int flag=0;
		DEBUG?:printf("looking up %s\n", name);
		char buf[13]={[0 ... 12]=0};
		memcpy(buf,name,min(strlen(name),12));
		for(int i=INODE_START;i<globalfUM->max_fObjs;i++){
			if (globalfUM->arr_fObjs[i]->active==1){
				if (memcmp(buf,globalfUM->arr_fObjs[i]->dfe->file_name,12)==0){
					//fuse_reply_err(req,EEXIST);
					flag=1;
					break;
				}
			}
		}
		if (!flag){
			DEBUG?:printf("creating %s\n",buf);
			struct fuse_entry_param e;
			memset(&e, 0, sizeof(e));
			e.ino=activatefObj(globalfUM,initdFEnt(0x33,buf,1,0));
			e.attr_timeout=100;
			e.entry_timeout=100;
			vmufs_stat(e.ino,&e.attr);
			//e.attr.st_size=0;
			//e.attr.st_mode=mode;
			fuse_reply_entry(req,&e);
		}else{
			fuse_reply_err(req,EEXIST);
		}
	}
}


static void vmufs_unlink(fuse_req_t req, fuse_ino_t parent,const char *name){
	int fObj_id=lookupName(name);
	if (fObj_id<0)
		fuse_reply_err(req,ENOENT);
	else{
		deactivatefObj(globalfUM,fObj_id);
		fuse_reply_err(req,0);
	}
}

static void vmufs_rename(fuse_req_t req, fuse_ino_t parent, const char *name,
			fuse_ino_t newparent, const char *newname){
	if (parent !=1){
		fuse_reply_err(req, ENOENT);
	}else{
		int sF=0;
		int dF=0;
		//name comparison, pray for null terminators
		char srcname[13] = {[0 ... 12]=0};
		memcpy(srcname , name, min(strlen(name), 12));
		
		char destname[13] = {[0 ... 12]=0};
		memcpy(destname , newname, min(strlen(newname),12));

		for(int i=INODE_START;i<globalfUM->max_fObjs;i++){
			if (globalfUM->arr_fObjs[i]->active==1){
				if (!sF && memcmp(srcname,globalfUM->arr_fObjs[i]->dfe->file_name,12)==0)
					sF=i;
				if (!dF && memcmp(destname,globalfUM->arr_fObjs[i]->dfe->file_name,12)==0)
					dF=i;
			}
			if (sF && dF)
				break;
		}
		if (!sF)
			fuse_reply_err(req,ENOENT);
		else if (!dF){
			//win
			dFEnt* sdfe=globalfUM->arr_fObjs[sF]->dfe;
			memcpy(&sdfe->file_name,destname,12);
			fuse_reply_err(req,0);
		}else //if (sF==dF){}
			fuse_reply_err(req,EEXIST);
	}
}

static void vmufs_write(fuse_req_t req, fuse_ino_t ino, const char *buf, size_t size, off_t off, struct fuse_file_info *fi){
	if (ino==1){
		fuse_reply_err(req,EISDIR);
	}else if (buf){
		DEBUG?:printf("\n\n***********BUFFER SIZE***********\nvmufs_write\n\nbuf:%lu *buf: %lu &buf:%lu\n*************************\n",sizeof(buf),sizeof(*buf),sizeof(&buf));
		DEBUG?:printf("WRITE\n\twriting ino: %lu,size: %lu, off_t: %lu\n",ino,size,off);
		DEBUG?:printf("buf_addr:\t%p\n",&(buf));

		fObj* fo=globalfUM->arr_fObjs[ino];
		//displayfObj(fo);
		int adjust=(off+size)%512;
		long altreqsize=adjust ? off+size+SZ_BLOCK-adjust : off+size;
		int altBlockCount=ceil(altreqsize/SZ_BLOCK);
		int currBlockCount=fo->dfe->file_sz;
		DEBUG?:printf("altreqsize:\t%li\n",altreqsize);
		DEBUG?:printf("Block Status:\t%i active of %i required.\n",currBlockCount,altBlockCount);
		DEBUG?:printf("currBlockCount:\t%li\n",currBlockCount);
		unsigned short reqBlocks=0;
		if (altBlockCount-currBlockCount>0){
			reqBlocks=altBlockCount-currBlockCount;
			DEBUG?:printf("ino %d: adding %d blocks...",ino,reqBlocks);
			if (reqBlocks<=globalfUM->avail_fBlks){
				//displayfObj(globalfUM->arr_fObjs[ino]);
				extendfObj(globalfUM,reqBlocks,ino);
				DEBUG?:printf("success!\n");
				//displayfObj(globalfUM->arr_fObjs[ino]);
			}else{
				DEBUG?:printf("FAIL.\n\tNot enough space for complete write!\n");
				DEBUG?:printf("\t%d available of %d required.\n",globalfUM->avail_fBlks,reqBlocks);
				if (globalfUM->avail_fBlks){
					extendfObj(globalfUM,globalfUM->avail_fBlks,ino);
				}else{
					DEBUG?:printf("No space remaining.\n");
					fuse_reply_err(req,ENOSPC);
					return;
				}
			}
		}else if (altBlockCount-currBlockCount<0){
			reqBlocks=altBlockCount;
			DEBUG?:printf("ino %d: removing %d blocks...",ino,currBlockCount-altBlockCount);
			reducefObj(globalfUM,reqBlocks,ino);
			DEBUG?:printf("success!\n");
		}
		DEBUG?:printf("can write %d bytes\n", fo->dfe->file_sz*SZ_BLOCK);
		size_t bytesWritten;
		bytesWritten=writeToFile(globalfUM,ino,buf,size,off);
		fuse_reply_write(req,bytesWritten);
	}else{
		printf("ino %d END",ino);
		fuse_reply_write(req,0);
	}
}

static struct fuse_lowlevel_ops vmufs_oper = {
	.lookup		= vmufs_lookup,
	.readdir	= vmufs_readdir,
	.getattr	= vmufs_getattr,
	.setattr 	= vmufs_setattr,
	.open		= vmufs_open,
	.read		= vmufs_read,
	.unlink 	= vmufs_unlink,
	.create 	= vmufs_create,
	//.getxattr	= vmufs_getxattr,
	.flush		= vmufs_flush,
	//.access		= vmufs_access,
	.mknod		= vmufs_mknod,
	.rename 	= vmufs_rename,
	.write 		= vmufs_write,
	
};




void test(){
	int i=0;
	while (i<globalfUM->max_fObjs && globalfUM->arr_fObjs[i]->active!=1)
		i++;
	if (globalfUM->arr_fObjs[i]->active==1){
		char* s=NULL;
		//s=buildFileBuffer(globalfUM->arr_fObjs[i],globalfUM);
		if (s){
			FILE* wptr;
			wptr=fopen("data.txt","wb");
			fwrite(s,1,(globalfUM->arr_fObjs[i]->dfe->file_sz*SZ_BLOCK),wptr);
			fclose(wptr);
			free(s);
			DEBUG?:printf("---------------------CURRENT----------------------------\n");
			displayfObj(globalfUM->arr_fObjs[i]);
			displayfUMan(globalfUM);
			DEBUG?:printf("----------------------DEACTIVATING----------------------------\n");
			deactivatefObj(globalfUM,i);
			DEBUG?:printf("---------------------NEW CURRENT-----------------------\n");
			displayfObj(globalfUM->arr_fObjs[i]);
			displayfUMan(globalfUM);
		}
	}else{
		DEBUG?:printf("no active files.\n");
	}
	int newID=0;
	char* name="foobar.txt";
	dFEnt* dfe=initdFEnt(0x33,name,4,0);
	DEBUG?:printf("---------------------CURRENT----------------------------\n");
	displayfUMan(globalfUM);
	DEBUG?:printf("----------------------ACTIVATING----------------------------\n");
	displaydFEnt(dfe);
	newID=activatefObj(globalfUM,dfe);
	displayfObj(globalfUM->arr_fObjs[newID]);
	displayfUMan(globalfUM);
	DEBUG?:printf("---------------------MODIFYING-----------------------\n");
	int a=extendfObj(globalfUM,10,newID);
	displayfObj(globalfUM->arr_fObjs[newID]);
	displayfUMan(globalfUM);
	DEBUG?:printf("----------------------DEACTIVATING----------------------------\n");
	deactivatefObj(globalfUM,newID);
	displayfUMan(globalfUM);
	DEBUG?:printf("---------------------NEW CURRENT-----------------------\n");
	displayfObj(globalfUM->arr_fObjs[newID]);
	displayfUMan(globalfUM);
}

int validateIMGFile(long* imgLen,FILE* fptr){
	//FILE *fp;
	int err=0;
	
	errno = 0;

	if (err=fseek(fptr, -SZ_BLOCK, SEEK_END))
		return err;
	*imgLen = (long)(ftell(fptr)+SZ_BLOCK);
	DEBUG?:printf("imglen: %ld\n",*imgLen);

	if ((*imgLen)%512){
		DEBUG?:printf("bad image len");
		errno = -EDOM;
		return errno;
	}
	return errno;
}

int main(int argc, char *argv[])
{

	int k,i;
	char* argv2[argc-1];
	char *filename = argv[1];
	long imgLen=0;
	FILE* fptr;
	if (!(fptr=fopen(filename,"rb"))){
		DEBUG?:printf(" .img file not found. terminating");
		return -ENOENT;
	}

	//find img file in args
	//filename = getIMGFileName(argc, argv);
	//validate img file
	if (validateIMGFile(&imgLen,fptr) < 0) {//|| (!validateRootBlk(rB,(*imgLen)/32))){
		fclose(fptr);
		//free(rB);
		return 0;
	}
	assembleGlobal(&globalfUM,filename,fptr);

	i=0;
	for (k=0; k < argc; k++){
		if(strstr(argv[k], ".img") == NULL)//{
			argv2[i++] =argv[k];
	}

	struct fuse_args args = FUSE_ARGS_INIT(argc-1, argv2);
	//fuse_opt_add_arg(&args, "-obig_writes");
	struct fuse_chan *ch;
	char *mountpoint;
	int err = -1;

	if (fuse_parse_cmdline(&args, &mountpoint, NULL, NULL) != -1 &&
		(ch = fuse_mount(mountpoint, &args)) != NULL) {
		struct fuse_session *se;
		se = fuse_lowlevel_new(&args, &vmufs_oper,
		sizeof(vmufs_oper), NULL);
		if (se != NULL) {
			if (fuse_set_signal_handlers(se) != -1) {
				fuse_session_add_chan(se, ch);
				err = fuse_session_loop(se);
				fuse_remove_signal_handlers(se);
				fuse_session_remove_chan(ch);
			}
			fuse_session_destroy(se);
		}
		fuse_unmount(mountpoint, ch);
	}
	free(mountpoint);
	fuse_opt_free_args(&args);
	//displayActiveFiles(globalfUM);
	writeToImage(globalfUM);
	destroyfUMan(globalfUM);
	return err ? 1 : 0;
}