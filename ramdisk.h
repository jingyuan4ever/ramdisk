/* include files */
#include <linux/vmalloc.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/utsname.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/ioctl.h>
#include <linux/errno.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <asm/string.h>
#include <asm/unistd.h>

#include"ramdisk_ioctl.h"

/*
#include <malloc.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
*/


/* SIZE ALL IN B */
#define BLOCK_SIZE 256

#define RAMDISK_SIZE 8192 * BLOCK_SIZE
#define SUPER_BLOCK_SIZE 1 * BLOCK_SIZE
#define INODE_ARRAY_SIZE 256 * BLOCK_SIZE
#define BITMAP_BLOCK_SIZE 4 * BLOCK_SIZE

#define INODE_SIZE 64 
#define INODE_BLOCK_POINTER_SIZE 4

#define DIR_FILENAME_SIZE 14

#define DIRENTRY_SIZE 16

/* NUMBER */
#define BITMAP_BIT_NUM 4 * BLOCK_SIZE * 8
#define BITMAP_BYTE_NUM 4 * BLOCK_SIZE

#define INODE_MAX_NUM (INODE_ARRAY_SIZE / INODE_SIZE)
#define INODE_BLOCK_POINTER_NUM 10

#define LOCATION_DIRECT_NUM 8
#define LOCATION_SINGLE_INDIRECT_NUM 1
#define LOCATION_DOUBLE_INDIRECT_NUM 1 

#define LOCATION_INDIRECT_POINTER_NUM 64

#define DIRENTRY_NUM (BLOCK_SIZE / DIRENTRY_SIZE) 

/* structure */

typedef struct superblock_t
{
	int freeblocknum;
	int freeinodenum;
}superblock_t;

typedef struct inode_t
{
	int status; //1 is in use, 0 is not in use
	char type[4]; //file type:"dir" or "reg" string
	long size;//file size: bytes
	char* location[10];//8 for direct, 1 for single-indirect and 1 for double-indirect
	int locationnum;
	char padding[8];// pad it to 64B;
}inode_t;

//directory entry which contains a filename and index_node_num
typedef struct direntry_t
{
	char filename[DIR_FILENAME_SIZE];
	short int inodenum;
}direntry_t;

typedef struct singleIndirect_t
{
	char* pointer[64];
}singleIndirect_t;

typedef struct doubleIndirect_t
{
	singleIndirect_t* pointer[64];
}doubleIndirect_t;


//jy:
#define MAX_OPEN_FILES 10
typedef struct fileDes_t
{
	char* filePosition;
	inode_t* inodePointer;
}fileDes;

typedef struct fileDesNode_t
{
	int pid;
	struct fileDesNode_t* next;
	fileDes fileDescriptorTable[MAX_OPEN_FILES];
}fileDesNode;

//jy

/* virables */
static char* ramdisk;
static superblock_t* sb;
static inode_t* inodearray;
static char* bitmapblock;
static char* freeblock;
static fileDesNode* fileDescriptorTableList;

static struct file_operations ramdiskOperations;
static struct proc_dir_entry *proc_entry;
static struct proc_dir_entry *proc_backup;
static char procfs_buffer[RAMDISK_SIZE];

/* functions */

/* init functions */
int initRamdisk();
void initSuperBlock(char*);
void initInodeArray(char*);
void initBitmapBlock(char*);
void initFileDescriptorTableList();
// void initInodeLocation(location_t);

/* test-print functions*/
void printInodeArray(int inodeindex);
void printSuperBlock();

/* directory entry functions*/
char* getFreeDirentryInBlock(char* blockAddr);
int createNewDirentry(int, char*, int);

/* block functions */
char* getFreeBlock();
char* allocateNewBlock(inode_t* inode);

/* bitmap functions*/
int getBit(unsigned int *value, int offset);
void setBit(unsigned int *value, int offset);
void clrBit(unsigned int *value, int offset);
int isBitmapBitFree(int bitmapbyteindex, int bitmapbitindex);
void setBitmapBit(int bitmapbyteindex, int bitmapbitindex);
void clrBitmapBit(int bitmapbyteindex, int bitmapbitindex);
/* inode functions */
int getFreeInode();
void freeInode(inode_t* ip);
int getInodeInBlock(char*, char*, char*, unsigned int);
int getInodeInDirEntry(int inodeIndex, char* fileName, char* type);
int getDirInodeNum(char* pathname);

/* pathname functions */
void splitDirAndFile(char* pathname, char** dir, char** filename);
int isPathnameValid(char* pathname, int* dirinodenum, char** filename);

/* file operations */
int my_creat(char* pathname);
int my_mkdir(char* pathname);
int my_open(char* pathname);
int my_close(int fd);
int my_read(int fd, char* address, int num_bytes);
int my_write(int fd, char* address, int num_bytes);
int my_lseek(int fd, int offset);
int my_unlink(char* pathname);
int my_readdir(int fd, char* address);

//int min(unsigned int a, unsigned int b);
char* getBlockOfInode(inode_t*, unsigned int);
int readInode(inode_t*, char*, unsigned int, unsigned int);
int writeInode(inode_t*, char*, unsigned int, unsigned int);
//int min(int a, int b);
