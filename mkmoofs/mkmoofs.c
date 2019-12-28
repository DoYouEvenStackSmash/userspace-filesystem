/*Filesystem
Memset 131072 bytes to 0

then set individual blocks

root block is 255

Free blocks should be marked with 0xFFFC
The end of chain marker is 0xFFFA
Each used block should contain either an end of chain marker, or a pointer to the next block in use for the file

It must initialize blocks 0-240 of the file to be all 0 bytes.
calloc(void* blah)

open binary
write 0 to file, 240*512
*/
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <stdint.h>
#include <string.h>
#define BLOCK_SIZE 512 //bytes
#define IMG_SIZE 512 //blocks
#define FAT_SIZE (IMG_SIZE/(BLOCK_SIZE/2))
#define FAT_START ((IMG_SIZE-1)-FAT_SIZE) //(512-1)-2)=509
#define DIR_END (FAT_START-1) //508
#define DIR_ENT_SIZE 32 //bytes
#define DIR_SIZE 29	//blocks 
#define DIR_START FAT_START-DIR_SIZE //509-29=480
#define NUM_USER_BLKS (DIR_SIZE*(BLOCK_SIZE/DIR_ENT_SIZE)) //29*(512/32)  

// while (user_blocs+(BLOCK_SIZE/DIR_ENT_SIZE)<DIR_END-1)

//each dir_block=16 user blocks
//root=511
//fat start=509
//dir end=508
//dir_start=480
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

// int offsetCalculator(int image_size){
// 	int low=0;
// 	int high=DIR_END;
// 	int dir_blocks=0
// 	while (low+(DIR_ENT_SIZE/BLOCK_SIZE)<high-1){
// 		low+=(DIR_ENT_SIZE/BLOCK_SIZE);
// 		high--;
// 		dir_blocks++;
// 	}
// 	return dir_blocks;
// }

void sample(char* name){
	int cap=128;
	FILE *fptr;
	fptr = fopen(name, "w");
	for(unsigned short i=0;i<cap;i++){
		for (unsigned short c=0;c<256;c++)
			fwrite(&i, sizeof(unsigned short),1,fptr);
	}
	fclose(fptr);
}

int main(int argc, char **argv){
	//sample(argv[1]);
	//return 0;

	FILE *fptr;
	unsigned char ZB[BLOCK_SIZE];
	memset(ZB,0,sizeof(ZB));
	fptr = fopen(argv[1], "w");
	// for(int i=0;i<256;i++){
	// 	for (unsigned short c=0;c<256;c++)
	// 		fwrite(c, sizeof(unsigned short),1,fptr);
	// }
	// fclose(fptr);

	int count=0;
	for(int i=0;i<DIR_START;i++){
		count++;
		fwrite(ZB,BLOCK_SIZE,1,fptr);
	}
	printf("user->dirStart count: %d\n",count);
	
	printf("Writing directory blocks\n");
	for(int i=0;i<DIR_SIZE;i++){
		count++;
		fwrite(ZB,BLOCK_SIZE,1,fptr);
	}
	printf("directory count: %d\n",count);
	unsigned short fat[IMG_SIZE];
	//memset(fat,0,IMG_SIZE);
	for(int i=0;i<IMG_SIZE;i++)
		fat[i]=65532;
	for (unsigned short i=DIR_END;i>DIR_START;i--){
		fat[i]=i-1;
	}
	fat[DIR_START]=65530;
	fat[IMG_SIZE-1]=65530;
	for(unsigned short i=FAT_START;i<(FAT_START+FAT_SIZE-1);i++)
		fat[i]=i+1;
	fat[FAT_START+FAT_SIZE-1]=65530;
	for(int i=0;i<IMG_SIZE;i++){
		if (!(i%8))
			printf("\n");
		//printf("%.04x ",fat[i]);
		printf("\t%d",fat[i]);
	}
	fwrite(fat,sizeof(unsigned short),IMG_SIZE,fptr);

	//return 0;
// 	//unsigned int userBlockCount = 200;
// 	//unsigned int unusedBlockCount = 41;
// 	//unsigned int singleLevelDirectoryBlockCount = 13;
// 	//unsigned int blockSize = 512;
// 	//unsigned int totalZeroField = blockSize * (userBlockCount+unusedBlockCount+singleLevelDirectoryBlockCount);
// 	//uint16_t bcd[8];
	struct rootBlock rb;
	memset(&rb,0,sizeof(rb));
	memset(rb.signature,0x55,sizeof(unsigned char)*16);
	rb.fat_sz=FAT_SIZE;
	rb.fat_start=FAT_START;
	rb.end_dir_blk=DIR_END;
	rb.dir_sz=DIR_SIZE;
	rb.num_user_blks=NUM_USER_BLKS;

	time_t rawtime;
	struct tm * timeinfo;
	time(&rawtime);
        timeinfo = localtime(&rawtime);
	//unsigned int timeint[8] ={timeinfo->tm_sec , timeinfo->tm_min, timeinfo->tm_hour, timeinfo->tm_mday, timeinfo->tm_mon, timeinfo->tm_year, timeinfo->tm_wday};
	unsigned int timeint[8]= {(timeinfo->tm_year+1900)/100,(timeinfo->tm_year+1900)%100, timeinfo->tm_mon+1, timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec, (timeinfo->tm_wday+6)%7};
	//unsigned char bcd[8];
	int i;
	for (i=0; i<8; i++){
		rb.BCD[i]= ((timeint[i]/10) << 4) + (timeint[i]%10);
	}
	fwrite(&rb,sizeof(rb),1,fptr);
	fclose(fptr);
	return 0;
// 	//130048
// 	//unsigned char timeSwap[24];
// 	//unsigned char dayswap[]
// 	//unsigned char timestampthing
// 	unsigned char zerobuffer[130048] ={[0 ... 130047] = 0x00};
	
// 	//256
// 	unsigned short fileAllocationTable[256]={[0 ... 240] = 0xFFFC, 0xFFFA , 0x00F1, 0x00F2, 0x00F3, 0x00F4, 0x00F5, 0x00F6, 0x00F7, 0x00F8, 0x00F9, 0x00FA, 0x00FB, 0x00FC, 0xFFFA, 0xFFFA};
	
// 	//BEGIN ROOT BLOCK
// 	//icon, userblocks, unused
// 	//434
// 	unsigned char rootBlockPt1[434] = {0x00, 0x00, 0xC8, 0x00, [4 ... 433] = 0x00};
	
// 	//starting block of FAT, size of FAT, LastBlock of directory, Size of Directory
// 	//8
// 	unsigned char rootBlockPt2[8] = {0xFE, 0x00, 0x01, 0x00, 0xFD, 0x00, 0x0D, 0x00 };
	
// 	//unused
// 	//14
// 	unsigned char rootBlockPt3[14] = {[0 ... 13] = 0x00};
	
// 	//faketimestamp
// 	//8
// 	unsigned char rootBlockPtTIME[8]={[0 ... 7] = 0x41};

// 	//colorbit, blue,green,red,alpha, unused(all 0 lul)
// 	//32
// 	unsigned char rootBlockPt5[32]={[0 ... 31] = 0x00};
	
// 	//filesystem signature
// 	//16
// 	unsigned char rootBlockPt6[16] = {[0 ... 15] = 0x55};
	
// 	//END ROOT BLOCK

// 	//
// 	//[0 ... 1] = 0x0D00, [2 ... 3] = 0xFD00, [4 ... 5] = 0x0100}
	

// 	//unsigned char imdirectorystill0[]={[0 ... 

// 	//time(&rawtime);
// 	//timeinfo = localtime(&rawtime);
// 	printf("Local time: %s", asctime(timeinfo));
// 	//bcd = {41,41,41,41,41,41,41,41};
// 	/*timeinfo.tm_sec;
// 	timeinfo.tm_min;
// 	timeinfo.tm_hour;
// 	timeinfo.tm_mday;
// 	timeinfo.tm_mon;
// 	timeinfo.tm_year;
// 	timeinfo.tm_wday;
// */
// 	for (int i =0; i < argc; ++i){
// 		printf("argv[%d]: %s\n", i, argv[i]);
// 	}
	
// 	fptr = fopen(argv[1], "w");
// 	if (fptr == NULL){
// 		//printf("cannot open file");
// 		exit(0);
// 	}else {
// 		printf("opening  %s", argv[1]);
// 	}
// 	//time(&timer)
// 	//prin
	
// 	fwrite(&zerobuffer, 1, 130048, fptr);
// 	fwrite(&fileAllocationTable, 1, 512, fptr);
// 	fwrite(&rootBlockPt6, 1, 16, fptr);
// 	fwrite(&rootBlockPt5, 1, 32, fptr);
// 	//fwrite(&rootBlockPtTIME, 1, 8, fptr);
// 	fwrite(&bcd,1,8,fptr);
// 	fwrite(&rootBlockPt3, 1, 14, fptr);
// 	fwrite(&rootBlockPt2, 1, 8, fptr);
// 	fwrite(&rootBlockPt1, 1, 434, fptr);

	
// 	fclose(fptr);
}
