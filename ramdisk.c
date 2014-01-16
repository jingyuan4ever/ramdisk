/*
#include <linux/malloc.h>
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
*/

#include "ramdisk.h"

/* Initialization */
int initRamdisk()
{
    char* superblockstart; //start of superblock
    char* inodearraystart; //start of inodearray
    char* blockbitmapstart; //start of bitmapblock
    char* freeblockstart; //start of freeblocks

    ramdisk = (char*)vmalloc(RAMDISK_SIZE);
    //ramdisk = (char*)malloc(RAMDISK_SIZE);

    //fail to initiate ramdisk
    if(ramdisk == NULL)
    {
        printk("Fail to initiate ramdisk");
        return -1;
    }

    //init
    superblockstart = ramdisk;
    // printk("super:%u\n", superblockstart);
    inodearraystart = superblockstart + SUPER_BLOCK_SIZE;
    // printk("inodearr:%u\n", inodearraystart);
    blockbitmapstart = inodearraystart + INODE_ARRAY_SIZE;
    // printk("blockbitmap:%u\n", blockbitmapstart);
    freeblockstart = blockbitmapstart + BITMAP_BLOCK_SIZE;
    // printk("freeblock:%u\n", freeblockstart);

    initSuperBlock(superblockstart);
    initInodeArray(inodearraystart);
    initBitmapBlock(blockbitmapstart);
    initFileDescriptorTableList();
    freeblock = freeblockstart;
    
    //root
    inodearray[0].status = 1;
    strcpy(inodearray[0].type, "dir");
    inodearray[0].size = 0; //a dir's size = 0
    inodearray[0].location[0] = getFreeBlock();
    inodearray[0].locationnum = 1;
    sb -> freeinodenum -= 1;

    return 1;

}

void initSuperBlock(char* superblockstart)
{
    int freeblocknum;
    int freeinodenum;

    freeblocknum = (RAMDISK_SIZE - SUPER_BLOCK_SIZE - INODE_ARRAY_SIZE - BITMAP_BLOCK_SIZE) / BLOCK_SIZE;
    freeinodenum = INODE_ARRAY_SIZE / INODE_SIZE;

    sb = (superblock_t*)superblockstart;
    sb -> freeblocknum = freeblocknum;
    sb -> freeinodenum = freeinodenum;
    // printk("initsb:%d %d\n",sb->freeblocknum, sb->freeinodenum);
    printk("initSuperBlock success");
}

void initInodeArray(char* inodearraystart)
{
    inodearray = (inode_t*)inodearraystart;

    int i,j;
    for (i = 0; i < INODE_MAX_NUM; i++) {
        strcpy(inodearray[i].type, "");
        inodearray[i].size = 0;
        inodearray[i].status = 0;
        for(j = 0; j < 10; j++){
            inodearray[i].location[j] = NULL;
        }
    }
    inodearray[i].locationnum = 0;
    printk("initInodeArray success");
}

void initBitmapBlock(char* blockbitmapstart)
{
    int i = 0;

    bitmapblock = blockbitmapstart;
    //printk("bitmapblock:%ld\n", bitmapblock);
    for (i = 0; i < BITMAP_BYTE_NUM ; i++) {
        bitmapblock[i] = 0; // 1 means this block is \\\\\\\in use, 0 is not in use
    }

    printk("initBitmap success");
}

void destroyRamdisk(void) {
    if (ramdisk) {
        vfree(ramdisk);
    }
    if (fileDescriptorTableList) {
        vfree(fileDescriptorTableList);
    }
    ramdisk = NULL;
    sb = NULL;
    inodearray = NULL;
}

/*
void printInodeArray(int inodeindex)
{
    printk("printInodeArray[%d]:\n", inodeindex);
    printk("type = %s, size = %ld, location = %ld\n", inodearray[inodeindex].type, inodearray[inodeindex].size, inodearray[inodeindex].location[0]);
    printk("singlein: %ld\n\n", inodearray[0].location[8]);
}
*/

char* getBlockOfInode(inode_t* ip, unsigned int index)
{
    int i,j;
    singleIndirect_t* singleAddr;
    doubleIndirect_t* doubleAddr;
    if(index < 8){
        if(ip->location[index] == NULL)
        {
            ip->location[index] = getFreeBlock();
        }
        return ip->location[index];
    }
    index-=8;
    if(index<64){
        if(ip->location[8] == NULL)
        {
            ip->location[8] = getFreeBlock();
        }
        if(ip->location[8] == NULL)
        {
            return NULL;
        }
        singleAddr = (singleIndirect_t*) ip->location[8];
        if(singleAddr->pointer[index]==NULL)
        {
            singleAddr->pointer[index] = getFreeBlock();
        }
        return singleAddr->pointer[index];
    }
    index-=64;
    if(index<64*64){
        if(ip->location[9] == NULL)
        {
            ip->location[9] = getFreeBlock();
        }
        if(ip->location[9] == NULL)
        {
            return NULL;
        }
        i = index/64;
        j = index%64;
        doubleAddr = (doubleIndirect_t*) ip->location[9];
        if(doubleAddr->pointer[i]==NULL)
        {
            doubleAddr->pointer[i] = getFreeBlock();
        }
        if(doubleAddr->pointer[i]==NULL)
        {
            return NULL;
        }
        singleAddr = (singleIndirect_t*)doubleAddr->pointer[i];
        if(singleAddr->pointer[j]==NULL)
        {
            singleAddr->pointer[j] = getFreeBlock();
        }
        return singleAddr->pointer[j];
    }
    return NULL;
}
int readBlock(char* block, char* buf, unsigned int offset, unsigned int length)
{
    if(offset>BLOCK_SIZE)
    {
        return -1;
    }
    if(offset+length>BLOCK_SIZE)
    {
        length=BLOCK_SIZE-offset;
    }
    memcpy(buf, block+offset, length);
    return length;
}
int writeBlock(char* block, char* buf, unsigned int offset, unsigned int length)
{
    if(offset>BLOCK_SIZE)
    {
        return -1;
    }
    if(offset+length>BLOCK_SIZE)
    {
        length=BLOCK_SIZE-offset;
    }
    memcpy(block+offset, buf, length);
    return length;
}
/*int min(unsigned int a, unsigned int b)
{
    if(a<=b)
        return a;
    return b;
}*/
int readInode(inode_t* ip, char* buf, unsigned int offset, unsigned int length)
{
    unsigned int byteToRead, readOnce, blockIndex, blockOffset;
    char* blockAddr, *cursor;
    if(offset>ip->size)
    {
        return -1;
    }
    if((offset+length)>(ip->size))
    {
        length=ip->size-offset;
    }
    blockIndex = offset/BLOCK_SIZE;
    blockOffset = offset%BLOCK_SIZE;
    byteToRead = length;
    cursor = buf;
    while(byteToRead>0)
    {
        //readOnce = min(byteToRead, BLOCK_SIZE-blockOffset);
        if(byteToRead > BLOCK_SIZE-blockOffset)
        {
            readOnce = BLOCK_SIZE-blockOffset;
        }
        else
        {
            readOnce = byteToRead;
        }
        readBlock(getBlockOfInode(ip, blockIndex), cursor, blockOffset, readOnce);
        byteToRead-=readOnce;
        cursor+=readOnce;
        blockIndex++;
        blockOffset = 0;
    }
    return length;
}

int writeInode(inode_t* ip, char* buf, unsigned int offset, unsigned int length)
{
    unsigned int byteToWrite, writeOnce, blockIndex, blockOffset;
    char*blockAddr, *cursor;
    if(offset>ip->size)
    {
        return -1;
    }
    blockIndex = offset/BLOCK_SIZE;
    blockOffset = offset%BLOCK_SIZE;
    byteToWrite = length;
    cursor = buf;
    while(byteToWrite>0)
    {
        //writeOnce = min(byteToWrite, BLOCK_SIZE-blockOffset);
        if(byteToWrite > BLOCK_SIZE-blockOffset)
        {
            writeOnce = BLOCK_SIZE-blockOffset;
        }
        else
        {
            writeOnce = byteToWrite;
        }
        writeBlock(getBlockOfInode(ip, blockIndex), cursor, blockOffset, writeOnce);
        byteToWrite-=writeOnce;
        cursor+=writeOnce;
        blockIndex++;
        blockOffset = 0;
    }
    if(offset+length>ip->size)
    {
        ip->size=offset+length;
    }
    return length;
}



char* getFreeDirentryInBlock(char* blockAddr) {
    int i;
    direntry_t* dirEntry;
    char* dirEntryAddr;

    dirEntryAddr = blockAddr;
    for(i = 0; i < DIRENTRY_NUM; i++) {
        dirEntry = (direntry_t*) dirEntryAddr;
        if (strcmp(dirEntry->filename, "/")!= 0 && dirEntry->inodenum == 0) {
            return dirEntryAddr;
        }
        dirEntryAddr = dirEntryAddr + DIRENTRY_SIZE;
    }
    return NULL;
}

char* getFreeBlock()
{
    // printk("getFreeBlock\n");
    int bitmapbyteindex;
    int bitmapbitindex;
    char* blockLocation;

    int blockindex = 0;

    for(bitmapbyteindex = 0; bitmapbyteindex < BITMAP_BYTE_NUM; bitmapbyteindex++)
    {
        for(bitmapbitindex = 0; bitmapbitindex < 8; bitmapbitindex++)
        {
            //printk("before bit:%d\n",isBitmapBitFree(bitmapbyteindex,bitmapbitindex));
            if(isBitmapBitFree(bitmapbyteindex, bitmapbitindex) == 0)
            {
                sb->freeblocknum--;
                setBitmapBit(bitmapbyteindex, bitmapbitindex);
                //printk("after bit:%d\n",isBitmapBitFree(bitmapbyteindex,bitmapbitindex));
                blockindex = bitmapbyteindex * 8 + bitmapbitindex;

                blockLocation = freeblock + BLOCK_SIZE * blockindex;
                //printk("freeblock:%ld\n", blockLocation);
                return blockLocation;
            }
        }
    }

    return NULL;
}

void freeBlock(char* block)
{
    if(block==NULL)
        return;
    int blockIndex = (block-freeblock)/BLOCK_SIZE;
    int bitmapbyteindex = blockIndex/8;
    int bitmapbitindex =blockIndex%8;
    clrBitmapBit(bitmapbyteindex, bitmapbitindex);
    sb->freeblocknum++;
}

/* bitmapblock functions */

int getBit(unsigned int *value, int offset)
{
    return *value & (0x01 << offset);
}

void setBit(unsigned int *value, int offset)
{
    *value |= (0x01 << offset);
}
void clrBit(unsigned int *value, int offset)
{
    *value &= ~(0x01 << offset);
}

int isBitmapBitFree(int bitmapbyteindex, int bitmapbitindex)
{
    //int* value = bitmapblock + bitmapbyteindex;
    unsigned int* value = bitmapblock + bitmapbyteindex;
    //printk("bitmapblockindex:%d\n", bitmapbyteindex);
    if(getBit(value, bitmapbitindex) == 0)
    {
        return 0;
    }

    return -1;
}

void setBitmapBit(int bitmapbyteindex, int bitmapbitindex)
{
    int *value = bitmapblock + bitmapbyteindex;
    setBit(value, bitmapbitindex);
}
void clrBitmapBit(int bitmapbyteindex, int bitmapbitindex)
{
    int *value = bitmapblock + bitmapbyteindex;
    clrBit(value, bitmapbitindex);
}

int getFreeInode()
{
    if(sb->freeinodenum == 0)
        return -1;
    int i = 0;
    for(i = 0; i < INODE_MAX_NUM; i++)
    {
        if(inodearray[i].status == 0)
        {
            inodearray[i].status = 1;
            sb->freeinodenum--;
            return i;
        }
    }
    return -1;
}
void freeInode(inode_t* ip)
{
    int size = ip->size;
    int blocknum = size/BLOCK_SIZE;
    int i;
    for(i=blocknum; i>7; i++)
    {
        char *block = getBlockOfInode(ip, i);
        freeBlock(block);
    }

    strcpy(ip->type, "");
    ip->size = 0;
    ip->status = 0;
    for(i = 0; i < 10; i++){
        freeBlock(ip->location[i]);
        ip->location[i] = NULL;
    }
    sb->freeinodenum++;
}

int createNewDirentry(int inodeIndex, char* fileName, int inodeNum)
{
    inode_t* ip = &inodearray[inodeIndex];
    direntry_t* entry = (direntry_t*)vmalloc(sizeof(direntry_t));
    strcpy(entry->filename, fileName);
    entry->inodenum = inodeNum;
    printk("cnd:%d,%d,%s,%d\n",inodeIndex,ip->size, entry->filename, entry->inodenum);
    int ret = writeInode(ip, entry, ip->size, sizeof(direntry_t));
    vfree(entry);
    //free(entry);
    return ret;
}


int getInodeInDirEntry(int inodeIndex, char* fileName, char* type) {
    inode_t* ip = &inodearray[inodeIndex];
    if(ip->size == 0)
    {
        return -1;
    }
    char* buf = (char*)vmalloc((ip->size));
    //char* buf = (char*)malloc((ip->size));
    char* cursor;
    char* dirEntryFilename;
    int dirEntryInodenum;
    char* dirEntryType;
    int n = ip->size/sizeof(direntry_t);
    direntry_t* dirEntry;
    int x = readInode(ip, buf, 0, ip->size);
    dirEntry = (direntry_t*)buf;
    int i;
    for(i=0;i<n;i++)
    {
        dirEntryFilename = dirEntry[i].filename;
        dirEntryInodenum = dirEntry[i].inodenum;
        dirEntryType = inodearray[dirEntryInodenum].type;

        if(strcmp(dirEntryFilename, fileName) == 0) {
            if ((strcmp("ign", type) == 0) || (strcmp(dirEntryType, type) == 0)) {
                
                vfree(buf);
                //free(buf);
                // printk("%d\n",dirEntryInodenum);
                return dirEntryInodenum;
            }
        }
    }
    vfree(buf);
    //free(buf);
    return -1;
}

int removeInodeInDirEntry(int inodeIndex, int fileinodenum) {
    inode_t* ip = &inodearray[inodeIndex];
    char* buf = (char*)vmalloc((ip->size));
    //char* buf = (char*)malloc((ip->size));
    char* dirEntryFilename;
    int dirEntryInodenum;
    char* dirEntryType;
    int n = ip->size/sizeof(direntry_t);
    direntry_t* dirEntry;
    int x = readInode(ip, buf, 0, ip->size);
    dirEntry = (direntry_t*)buf;
    int i;
    for(i=0;i<n;i++)
    {
        dirEntryFilename = dirEntry[i].filename;
        dirEntryInodenum = dirEntry[i].inodenum;

        if(dirEntryInodenum == fileinodenum) {
            break;
        }
    }
    memmove(buf+i*sizeof(direntry_t), buf+(i+1)*sizeof(direntry_t), ip->size-i*sizeof(direntry_t));
    writeInode(ip, buf, 0, ip->size-sizeof(direntry_t));
    vfree(buf);
    //free(buf);
    return -1;
}

//split pathname to parents and filename
void splitDirAndFile(char* pathname, char** dir, char** filename)
{
    char* lastslashptr; //string from the last '/'
    int lastslash; //location of the last '/'
    int filenamelen;
    int dirlen;
    int length;
    char* pathnameCopy;
    char* temp;

    length = strlen(pathname);
    pathnameCopy = (char*)vmalloc(sizeof(char)*(length+1));
    //pathnameCopy = (char*)malloc(sizeof(char)*(length+1));
    strcpy(pathnameCopy, pathname);

    lastslashptr = strrchr(pathnameCopy, '/');
    lastslash = lastslashptr - pathnameCopy;

    dirlen = sizeof(char) * (lastslash + 2);
    filenamelen = sizeof(char) * strlen(lastslashptr);

    *dir = (char*)vmalloc(dirlen);
    //*dir = (char*)malloc(dirlen);
    memcpy(*dir, pathnameCopy, lastslash + 1);
    temp = *dir;
    temp+=(lastslash+1);
    *temp='\0';

    
    *filename = (char*)vmalloc(filenamelen);
    //*filename = (char*)malloc(filenamelen);
    memcpy(*filename, lastslashptr + 1, filenamelen);
    vfree(pathnameCopy);
    //free(pathnameCopy);
}

int getDirInodeNum(char* pathname)
{
    // printk("%s\n", pathname);
    //printk("enter getDirInodeNum:\n");
    char* fullpath;
    char* dirs;
    int inodenum = 0;
    int dirinodenum = 0;
    
    //root's inodenum = 0
    if(strcmp(pathname, "/") == 0)
    {
        return 0;
    }

    fullpath = pathname;
    dirs = strsep(&fullpath, "/"); //delete the first '/' from the pathname
    dirs = strsep(&fullpath, "/"); //get the first dir
    
    // printk("dirs: %s\n", dirs);
    
    while(strlen(dirs) > 0)
    {
        dirinodenum = getInodeInDirEntry(inodenum, dirs, "dir");
        
        if(dirinodenum == -1)
        {
            return -1;
        }
        else
        {
            inodenum = dirinodenum;
        }

        dirs = strsep(&fullpath, "/");
    }

    return inodenum;
}

//get the parent dir's inode num in the inode array, -1 means the parent's dir is not valid
int isPathnameValid(char* pathname, int* dirinodenum, char** filename)
{
    //printk("enter isPathnameValid function:\n");
    char* dir;
    char* file;

    //check if pathname begins with '/'
    if(strlen(pathname) > 0 && pathname[0] != '/')
    {
        //printk("Pathname doesn't begin with '/'\n");
        return -1;
    }

    splitDirAndFile(pathname, &dir, &file);

    // printk("dir:%s, file\n", dir);   

    /*get the inodenum of the dir from the direntry table */
    *dirinodenum = getDirInodeNum(dir);
    *filename = file;

    if(*dirinodenum == -1)
    {
        //printk("Cannot get parent.\n");
        return -1;
    }
    return 0;
}
/* FD functions */
int getpid()
{
    return current->pid;
    //return 1;
}
void initFileDescriptorTableList()
{
    fileDesNode* head;
    head = (fileDesNode*)vmalloc(sizeof(fileDesNode));
    //head = (fileDesNode*)malloc(sizeof(fileDesNode));
    head->pid=0;
    head->next=NULL;
    fileDescriptorTableList = head;
}
fileDesNode* findFileDescriptorTableByPid(int pid)
{
    //if exist, return
    fileDesNode* currentNode = fileDescriptorTableList;
    while(currentNode)
    {
        if(currentNode->pid == pid)
        {
            return currentNode;
        }
        currentNode = currentNode->next;
    }
    //else, create
    fileDesNode* entry = (fileDesNode*)vmalloc(sizeof(fileDesNode));
    //fileDesNode* entry = (fileDesNode*)malloc(sizeof(fileDesNode));
    int i;
    for(i=0;i<MAX_OPEN_FILES;i++)
    {
        entry->fileDescriptorTable[i].inodePointer = NULL;
        entry->fileDescriptorTable[i].filePosition = -1;
    }
    entry->pid = pid;
    entry->next = fileDescriptorTableList;
    fileDescriptorTableList->next = entry;
    return entry;
}
int findFileDescriptor(fileDesNode* fdn)
{
    int i;
    for(i=0;i<MAX_OPEN_FILES;i++)
    {
        if(fdn->fileDescriptorTable[i].filePosition == -1)
            return i;
    }
    return -1;
}
int addFileDescriptorTableEntry(int pid, int inodeIndex)
{
    fileDesNode* fdn = findFileDescriptorTableByPid(pid);
    int index;
    index = findFileDescriptor(fdn);
    if(index==-1)
    {
        //printk("Fail to add file descriptor: exceeding MAX_OPEN_FILES\n");
        return -1;
    }
    fdn->fileDescriptorTable[index].inodePointer = &inodearray[inodeIndex];
    fdn->fileDescriptorTable[index].filePosition = 0;
    return index;
}

/* --------------file operations------------------ */

int my_creat(char* pathname)
{
    int freeinodeindex;
    int dirinodeindex;
    char* freeblockptr;
    char* filename;

    /* check if free inode exists */
    if(sb -> freeinodenum == 0)
    {
        printk("Fail to create regular file: there is no free inode\n");
        // free(filename);
        return -1;
    }

    /* check if free block exists */
    if(sb -> freeblocknum == 0)
    {
        printk("Fail to create regular file: there is no free block\n");
        // free(filename);
        return -1;
    }

    /* check if pathname is valid */
    if(isPathnameValid(pathname, &dirinodeindex, &filename) == -1)
    {
        printk("Fail to create regular file: the pathname %s is invalid\n", pathname);
        // free(filename);
        return -1;
    }

    if(getInodeInDirEntry(dirinodeindex, filename, "reg") != -1)
    {
        printk("Fail to create regular file: file already exists\n");
        // free(filename);
        return -1;
    }
    
    
    /* allocate a free inode and block*/
    freeinodeindex = getFreeInode();
    inodearray[freeinodeindex].status = 1;
    strcpy(inodearray[freeinodeindex].type, "reg");
    inodearray[freeinodeindex].size = 0;
    inodearray[freeinodeindex].location[0] = getFreeBlock();
    inodearray[freeinodeindex].locationnum = 1;
    

    /* edit parent's direntry */
    createNewDirentry(dirinodeindex, filename, freeinodeindex);

    printk("rd_creat: filename: %s\n", filename);
    // free(filename);
    return 1;
}
int my_mkdir(char* pathname)
{
    int freeinodeindex;
    int dirinodeindex;
    char* freeblockptr;
    char* filename;

    /* check if free inode exists */
    if(sb -> freeinodenum == 0)
    {
        printk("Fail to create directory file: there is no free inode\n");
        // free(filename);
        return -1;
    }

    /* check if free block exists */
    if(sb -> freeblocknum == 0)
    {
        printk("Fail to create directory file: there is no free block\n");
        // free(filename);
        return -1;
    }

    /* check if pathname is valid */
    if(isPathnameValid(pathname, &dirinodeindex, &filename) == -1)
    {
        printk("Fail to create directory file: the pathname %s is invalid\n", pathname);
        // free(filename);
        return -1;
    }

    if(getInodeInDirEntry(dirinodeindex, filename, "dir") != -1)
    {
        printk("Fail to create directory file: file already exists\n");
        // free(filename);
        return -1;
    }
    
    
    /* allocate a free inode and block*/
    freeinodeindex = getFreeInode();
    inodearray[freeinodeindex].status = 1;
    strcpy(inodearray[freeinodeindex].type, "dir");
    inodearray[freeinodeindex].size = 0;
    inodearray[freeinodeindex].location[0] = getFreeBlock();
    inodearray[freeinodeindex].locationnum = 1;
    

    /* edit parent's direntry */
    createNewDirentry(dirinodeindex, filename, freeinodeindex);

    printk("Create directory %s successfully.\n", filename);
    // free(filename);
    return 1;
    return 0;
}

int my_open(char* pathname)
{
    int pid = getpid();
    int fileinodeindex;
    int dirinodeindex;
    char* filename;
    int fd;
    if(strcmp(pathname, "/") == 0)
    {
        fd = addFileDescriptorTableEntry(pid, 0);
        printk("Open file %s successfully.\n",pathname);
        return fd;
    }

    /* check if pathname is valid */
    if(isPathnameValid(pathname, &dirinodeindex, &filename) == -1)
    {
        printk("Fail to open file: the pathname is invalid\n");
        return -1;
    }
    fileinodeindex = getInodeInDirEntry(dirinodeindex, filename, "ign");
    if(fileinodeindex == -1)
    {
        printk("Fail to open file %s: file not found\n",filename);
        return -1;
    }
    fd = addFileDescriptorTableEntry(pid, fileinodeindex);

    printk("Open file %s successfully.\n",pathname);
    return fd;
    //return 1;
}
int my_close(int fd)
{
    int pid = getpid();
    if(fd<0)
    {
        printk("Fail to close: invalid file descriptor %d\n", fd);
        return -1;
    }
    fileDesNode* fdn = findFileDescriptorTableByPid(pid);
    if(fdn->fileDescriptorTable[fd].inodePointer == NULL)
    {
        printk("Fail to close: file %d is not opend.\n", fd);
        return -1;
    }
    fdn->fileDescriptorTable[fd].inodePointer = NULL;
    fdn->fileDescriptorTable[fd].filePosition = -1;
        printk("Close file %d successfully.\n", fd);
    return 0;
}

int my_read(int fd, char* address, int num_bytes)
{
    inode_t* ip;
    int pid = getpid();
    int filePosition;
    int length;
    if(fd<0)
    {
        printk("Fail to read: invalid file descriptor %d\n", fd);
        return -1;
    }

    fileDesNode* fdn = findFileDescriptorTableByPid(pid);
    if(fdn->fileDescriptorTable[fd].inodePointer == NULL)
    {
        printk("Fail to read: file %d is not opend.\n", fd);
        return -1;
    }

    if(strcmp(fdn->fileDescriptorTable[fd].inodePointer->type,"reg") !=0)
    {
        printk("Fail to read: file %d is not a regular file.\n", fd);
        return -1;
    }

    ip = fdn->fileDescriptorTable[fd].inodePointer;
    if(num_bytes>ip->size)
    {
        num_bytes = ip->size;
    }
    filePosition = fdn->fileDescriptorTable[fd].filePosition;
    length = readInode(ip, address, filePosition, num_bytes);
    fdn->fileDescriptorTable[fd].filePosition += length;
    return length;
}

int my_write(int fd, char* address, int num_bytes)
{ 
    inode_t* ip;
    int pid = getpid();
    int filePosition;
    int length;

    if (fd < 0) 
    {
        printk("Fail to write: invalid file descriptor %d\n", fd);
        return -1;
    }

    fileDesNode* fdn = findFileDescriptorTableByPid(pid);

    // Check to see if the calling process has opened the file in question.
    if(fdn->fileDescriptorTable[fd].inodePointer == NULL)
    {
        printk("Fail to write: file %d is not opend.\n", fd);
        return -1;
    }

    ip = fdn->fileDescriptorTable[fd].inodePointer;

    if (strcmp(ip->type, "reg") != 0) {
        printk("Fail to write: attempted to write to a directory file\n");
        return -1;
    }

    filePosition = fdn->fileDescriptorTable[fd].filePosition;
    length = writeInode(ip, address, filePosition, num_bytes);

    printk("Write file successfully.\n");
    return length;
}

int my_lseek(int fd, int offset)
{
    int pid = getpid();
    int fileSize;
    if(fd<0)
    {
        printk("Fail to lseek: invalid file descriptor %d\n", fd);
        return -1;
    }

    fileDesNode* fdn = findFileDescriptorTableByPid(pid);
    if(fdn->fileDescriptorTable[fd].inodePointer == NULL)
    {
        printk("Fail to lseek: file %d is not opend.\n", fd);
        return -1;
    }

    if(strcmp(fdn->fileDescriptorTable[fd].inodePointer->type,"reg") !=0)
    {
        printk("Fail to lseek: file %d is not a regular file.\n", fd);
        return -1;
    }

    fileSize = fdn->fileDescriptorTable[fd].inodePointer->size;
    if(offset<0)
        offset = 0;
    if(offset>fileSize)
        offset = fileSize;
    fdn->fileDescriptorTable[fd].filePosition = offset;
    printk("Lseek file %d successfully.\n", fd);

    return 0;
}

int my_unlink(char* pathname)
{
    int pid = getpid();
    int fileinodeindex;
    int dirinodeindex;
    char* filename;
    int fd;
    inode_t* ip;
    if (strcmp(pathname, "/") == 0) {
        printk("Fail to unlink: cannot unlink root directory!\n");
        return -1;
    }

    if(isPathnameValid(pathname, &dirinodeindex, &filename) == -1)
    {
        printk("Fail to unlink: the pathname %s is invalid\n", pathname);
        // free(filename);
        return -1;
    }

    fileinodeindex = getInodeInDirEntry(dirinodeindex, filename, "ign");
    if(fileinodeindex == -1)
    {
        printk("Fail to unlink file %s: file not found\n",filename);
        return -1;
    }
    ip = &(inodearray[fileinodeindex]);
    if(strcmp(ip->type, "dir") == 0 && ip->size != 0) {
        printk("Fail to unlink file %s: cannot unlink non-empty directory\n");
        return -1;
    }
    removeInodeInDirEntry(dirinodeindex, fileinodeindex);
    freeInode(ip);
    printk("Unlink %s successfully.\n", pathname);
    return 0;
}
int my_readdir(int fd, char* address)
{
    inode_t* ip;
    int pid = getpid();
    int filePosition;
    if(fd<0)
    {
        printk("Fail to readdir: invalid file descriptor %d\n", fd);
        return -1;
    }

    fileDesNode* fdn = findFileDescriptorTableByPid(pid);
    if(fdn->fileDescriptorTable[fd].inodePointer == NULL)
    {
        printk("Fail to readdir: file %d is not opend.\n", fd);
        return -1;
    }

    if(strcmp(fdn->fileDescriptorTable[fd].inodePointer->type,"dir") !=0)
    {
        printk("Fail to readdir: file %d is a regular file.\n", fd);
        return -1;
    }

    ip = fdn->fileDescriptorTable[fd].inodePointer;
    printk("size:%d\n",ip->size);
    filePosition = fdn->fileDescriptorTable[fd].filePosition;
    if(ip->size == 0)
    {        
        printk("Readdir %d is empty.\n", fd);
        return 0;
    }
    if(filePosition==ip->size)
    {
        printk("Readdir %d is EOF.\n", fd);
        return 0;
    }
    readInode(ip, address, filePosition, sizeof(direntry_t));
    printk("Readdir %d successfully.\n", fd);

    int length = readInode(ip, address, filePosition, sizeof(direntry_t));
    fdn->fileDescriptorTable[fd].filePosition += length;

    return 1;
}
/*
void createFiles()  
{    
    char pathname[DIR_FILENAME_SIZE];    
    int i = 0;
    for (i = 0; i < 50; i++)        
    {      
         sprintk(pathname, "/file_c_%d", i);          
         my_creat(pathname);    
        
        printk("%d\n",sb->freeinodenum);
    }
}*/

/*
int main()
{
    initRamdisk();
    createFiles();
    char c[11];
    c[10]='\0';
    int fd = my_open("/file_c_1");
    my_write(fd,"this is a test, and this i need to do some test", 30);
    my_read(fd, c, 10);
    printk("%s\n", c);
    my_lseek(fd, 2);
    my_read(fd, c, 10);
    printk("%s\n", c);
    my_close(fd);
    direntry_t* dir = (direntry_t*)malloc(sizeof(direntry_t));
    fd = my_open("/");
    my_readdir(fd, dir);
    printk("root: %s, %d\n", dir->filename, dir->inodenum);
    my_close(fd);
    my_unlink("/file_c_1");
    my_unlink("/file_c_1");
    my_unlink("/file_c_2");
    my_unlink("/file_c_2");
    free(ramdisk);
}
*/

/*----------------------ioctl------------------*/
static int ramdisk_ioctl(struct inode *inode, struct file *file, 
                         unsigned int cmd, unsigned long arg) {
    
    // Stores our params
    ioctl_rd params;
    char* path;
    char* kernelAddress;
    int size;
    int ret;
    path = NULL;

    // Create a copy of the params object
    copy_from_user(&params, (ioctl_rd *)arg, sizeof(ioctl_rd));

    // Determine which command was issued
    switch (cmd) {
        case IOCTL_RD_CREAT:
            // Create a copy of pathname from user space
            size = sizeof(char) * (params.pathnameLength + 1);
            path = (char*)vmalloc(size);
            copy_from_user(path, params.pathname, size);
    
                        ret = my_creat(path);
            vfree(path);
            return ret;
            break;
   
        case IOCTL_RD_MKDIR:
            // Create a copy of pathname from user space
            size = sizeof(char) * (params.pathnameLength + 1);
            path = (char*)vmalloc(size);
            copy_from_user(path, params.pathname, size);
                        
            ret = my_mkdir(path);
            vfree(path);
            return ret;
            break;
    
        case IOCTL_RD_OPEN:
            // Create a copy of pathname from user space
            size = sizeof(char) * (params.pathnameLength + 1);
            path = (char*)vmalloc(size);
            copy_from_user(path, params.pathname, size);
                        ret = my_open(path);
            vfree(path);
            return ret;
            break;
        
        case IOCTL_RD_CLOSE:
                        ret = my_close(params.fd);
            return ret;
            break;

        case IOCTL_RD_READ:
                        kernelAddress = (char*) vmalloc(params.num_bytes);
                        ret = my_read(params.fd, kernelAddress, params.num_bytes);
                        if (ret != -1) {
                            copy_to_user(params.address, kernelAddress, ret);
                    }
                        vfree(kernelAddress);
            return ret;
            break;
    
        case IOCTL_RD_WRITE:
                        kernelAddress = (char*) vmalloc(params.num_bytes);
            copy_from_user(kernelAddress, params.address, (unsigned long) params.num_bytes);
                        ret = my_write(params.fd, kernelAddress, params.num_bytes);
            vfree(kernelAddress);
            return ret;
            break;
    
        case IOCTL_RD_LSEEK:
                        ret = my_lseek(params.fd, params.offset);
            return ret;
            break;

        case IOCTL_RD_UNLINK:
            // Create a copy of pathname from user space
            size = sizeof(char) * (params.pathnameLength + 1);
            path = (char*)vmalloc(size);
            copy_from_user(path, params.pathname, size);

                        ret = my_unlink(path);
            vfree(path);
            return ret;
            break;

        case IOCTL_RD_READDIR:
            kernelAddress = (char*) vmalloc(16);
            ret = my_readdir(params.fd, kernelAddress);
            copy_to_user(params.address, kernelAddress, 16);
            vfree(kernelAddress);
            return ret;
            break;

        default:
            return -EINVAL;
            break;
  }
  return 0;
}


/* Initialization point for the module */
static int __init init_routine(void) {
    int ret;
    printk("Loading ramdisk module...\n");

    // Set the entry point for the ramdisk_ioctl
    ramdiskOperations.ioctl = ramdisk_ioctl;

    // Start create proc entry
    proc_entry = create_proc_entry("ramdisk_ioctl", 0444, NULL);
        proc_backup = create_proc_entry("ramdisk_backup", 0644, NULL);
    if (!proc_entry || !proc_backup) {
        printk("rd: Error creating /proc entry.\n");
        return 1;
    }

    //proc_entry->owner = THIS_MODULE;
    proc_entry->proc_fops = &ramdiskOperations;

    // Initialize ramdisk
    ret = initRamdisk();
    printk("Ramdisk module loaded!\n");

    return ret;
}


/* Exit point for the module */
static void __exit exit_routine(void) {
    printk("Removing ramdisk module...\n");

    // Destroy our ramdisk
    destroyRamdisk();

    // Remove the entry from /proc
    remove_proc_entry("ramdisk_ioctl", NULL);
        remove_proc_entry("ramdisk_backup", NULL);

    printk("Ramdisk module removed!\n");
    return;
}


module_init(init_routine);
module_exit(exit_routine);

