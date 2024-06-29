#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>

#define stat xv6_stat  // avoid clash with host struct stat
#include "kernel/types.h"
#include "kernel/fs.h"
#include "kernel/stat.h"
#include "kernel/param.h"

#ifndef static_assert
#define static_assert(a, b) do { switch (0) case 0: case (a): ; } while (0)
#endif

#ifndef SNU
// NINODES moved to kernel/param.h
#define NINODES 200
#endif

#define min(a, b) ((a) < (b) ? (a) : (b))

// Disk layout:
// [ boot block | sb block | log | fat blocks | inode blocks | data blocks ]

//SNU
int nfatblocks = (FSSIZE * 4)/ BSIZE + 1;
int ninodeblocks = NINODES / IPB + 1; // 200 / 16 + 1 = 13 -> 200 / 64 + 1 = 4
int nlog = LOGSIZE; //30; 
int nmeta;    // Number of meta blocks (boot, sb, nlog, inode, bitmap)
int nblocks;  // Number of data blocks

int fsfd;
struct superblock sb;
char zeroes[BSIZE];
uint freeinode = 1;


void wsect(uint, void*);
void winode(uint, struct dinode*);
void rinode(uint inum, struct dinode *ip);
void rsect(uint sec, void *buf);
uint ialloc(ushort type);
void iappend(uint inum, void *p, int n);
void die(const char *);
void initfat(void);
int falloc(uint linkblock);
int rfat(uint startblock);
void print_fatblock();
void print_inodeblocks();
void read_dirblocks(uint blknum);
void read_superblock();

// convert to riscv byte order
ushort
xshort(ushort x)
{
  ushort y;
  uchar *a = (uchar*)&y;
  a[0] = x;
  a[1] = x >> 8;
  return y;
}

uint
xint(uint x)
{
  uint y;
  uchar *a = (uchar*)&y;
  a[0] = x;
  a[1] = x >> 8;
  a[2] = x >> 16;
  a[3] = x >> 24;
  return y;
}

int
main(int argc, char *argv[])
{
  int i, cc, fd;
  //int i, fd;
  uint rootino, inum, off;
  struct dirent de;
  char buf[BSIZE];
  struct dinode din;


  static_assert(sizeof(int) == 4, "Integers must be 4 bytes!");

  if(argc < 2){
    fprintf(stderr, "Usage: mkfs fs.img files...\n");
    exit(1);
  }

  assert((BSIZE % sizeof(struct dinode)) == 0);
  assert((BSIZE % sizeof(struct dirent)) == 0);

  fsfd = open(argv[1], O_RDWR|O_CREAT|O_TRUNC, 0666);
  if(fsfd < 0)
    die(argv[1]);

  // 1 fs block = 1 disk sector
  //nmeta = 2 + nlog + ninodeblocks + nbitmap;
  nmeta = 2 + nlog + nfatblocks + ninodeblocks;
  nblocks = FSSIZE - nmeta;

  //sb.magic = FSMAGIC;
  sb.magic = FSMAGIC_FATTY; //SNU
  sb.size = xint(FSSIZE);
  sb.nblocks = xint(nblocks);
  sb.ninodes = xint(NINODES);
  sb.nlog = xint(nlog);
  sb.logstart = xint(2);
  //SNU
  sb.nfat = xint(nfatblocks);
  sb.fatstart = xint(2+nlog);
  sb.freehead = xint(nmeta);
  sb.freeblks = xint(nblocks);
  sb.inodestart = xint( 2 + nlog + nfatblocks );
  //printf("nmeta %d (boot, super, log blocks %u, fat blocks %u, inode blocks %u) blocks %d total %d\n", nmeta, nlog, nfatblocks ,ninodeblocks, nblocks, FSSIZE);


  for(i = 0; i < FSSIZE; i++)
    wsect(i, zeroes);

  //allocate the superblock
  memset(buf, 0, sizeof(buf));
  memmove(buf, &sb, sizeof(sb));
  wsect(1, buf);

  //1. allocate the root inode (DIR)
  rootino = ialloc(T_DIR);
  assert(rootino == ROOTINO);

  //2. initialize the fat blocks
  initfat();

  //3. directory entry for root file
  bzero(&de, sizeof(de));
  de.inum = xshort(rootino);
  strcpy(de.name, ".");
  iappend(rootino, &de, sizeof(de)); //append the . directory entry to the root data block

  bzero(&de, sizeof(de)); 
  de.inum = xshort(rootino);
  strcpy(de.name, "..");
  iappend(rootino, &de, sizeof(de)); //append the .. directory entry to the root data block


  //4. add files
  for(i = 2; i < argc; i++){
    // get rid of "user/"
    char *shortname;
    if(strncmp(argv[i], "user/", 5) == 0)
      shortname = argv[i] + 5;
    else
      shortname = argv[i];
    
    assert(index(shortname, '/') == 0);

    if((fd = open(argv[i], 0)) < 0)
      die(argv[i]);

    // Skip leading _ in name when writing to file system.
    // The binaries are named _rm, _cat, etc. to keep the
    // build operating system from trying to execute them
    // in place of system binaries like rm and cat.
    if(shortname[0] == '_')
      shortname += 1;

    inum = ialloc(T_FILE);

    bzero(&de, sizeof(de));
    de.inum = xshort(inum);
    strncpy(de.name, shortname, DIRSIZ);
    iappend(rootino, &de, sizeof(de));

    //int total_filesize = 0;
    while((cc = read(fd, buf, sizeof(buf))) > 0){
      //total_filesize += cc;
      iappend(inum, buf, cc);
    }
    //printf("total_filesize: %d\n", total_filesize);
    //fix size of the file
    // rinode(inum, &din);
    // off = xint(din.size);
    // off = ((off/BSIZE) + 1) * BSIZE;
    // din.size = xint(off);
    //winode(inum, &din);
    close(fd);
  }

  // fix size of root inode dir
  rinode(rootino, &din);
  off = xint(din.size);
  off = ((off/BSIZE) + 1) * BSIZE;
  din.size = xint(off);
  winode(rootino, &din);
  print_inodeblocks();
  //print_fatblock();
  //read_dirblocks(44);
  //read_superblock();
  exit(0);
}

void
wsect(uint sec, void *buf)
{
  if(lseek(fsfd, sec * BSIZE, 0) != sec * BSIZE)
    die("lseek");
  if(write(fsfd, buf, BSIZE) != BSIZE)
    die("write");
}

void
winode(uint inum, struct dinode *ip)
{
  char buf[BSIZE];
  uint bn;
  struct dinode *dip;

  bn = IBLOCK(inum, sb);
  rsect(bn, buf);
  dip = ((struct dinode*)buf) + (inum % IPB);
  *dip = *ip;
  wsect(bn, buf);
}

void
rinode(uint inum, struct dinode *ip)
{
  char buf[BSIZE];
  uint bn;
  struct dinode *dip;

  bn = IBLOCK(inum, sb);
  rsect(bn, buf);
  dip = ((struct dinode*)buf) + (inum % IPB);
  *ip = *dip;
}

void
rsect(uint sec, void *buf)
{
  if(lseek(fsfd, sec * BSIZE, 0) != sec * BSIZE)
    die("lseek");
  if(read(fsfd, buf, BSIZE) != BSIZE)
    die("read");
}

uint
ialloc(ushort type)
{
  uint inum = freeinode++;
  struct dinode din;

  bzero(&din, sizeof(din));
  din.type = xshort(type);
  din.nlink = xshort(1);
  din.size = xint(0);
  din.startblk = xint(0); //initialize with the startblk 0  //SNU
  winode(inum, &din);
  return inum;
}



void
iappend(uint inum, void *xp, int n)
{ 
  char *p = (char*)xp;


  uint startblk, lastblk, off, filesize;
  struct dinode din;
  char buf[BSIZE];

  rinode(inum, &din);
  startblk = xint(din.startblk); // startblk of the file
  filesize = xint(din.size); //File size


  off = filesize % BSIZE; //offset of the file. start position of the next write

  //1. Find the last block of the file which we start to write
  if(filesize == 0){ // if the file is empty
    startblk = falloc(0);
    din.startblk = xint(startblk);
    lastblk = startblk;
  }
  else if(off == 0){ //if the file size is multiple of BSIZE
    lastblk = rfat(startblk);
    lastblk = falloc(lastblk);
    off = 0;
  }
  else{
    lastblk = rfat(startblk);
  }
  //printf("inum: %d, requested_size: %d, startblk: %d, lastblk: %d, off: %d, filesize: %d\n", inum, n, startblk, lastblk, off, filesize);
  //2. Write the data to the blocks
  while(n > 0){
    //read the last block
    rsect(lastblk, buf);
    //print the buf
    //calculate the remaining space of the block
    int n1 = min(n, BSIZE - off);
    //copy the data to the block
    bcopy(p, buf + off, n1);
    //write the block
    wsect(lastblk, buf);
    //update the variables
    n -= n1;
    p += n1;
    filesize += n1;
    off = (off + n1) % BSIZE;
    //printf("n1 %d, n %d, filesize %d\n", n1, n, filesize);
    //allocate the next block
    if(n > 0){
      lastblk = falloc(lastblk);
    }
  }
  //fix the size of the file
  din.size = xint(filesize);
  winode(inum, &din);
}

void
die(const char *s)
{
  perror(s);
  exit(1);
}

void initfat(void)
{
  int i;
  uint fatblock = sb.fatstart; //idx of start of fatblock
  uint freehead = sb.freehead; //head idx of free list

  uint fs_size = sb.size;
  //printf("fs_size: %d\n", fs_size);
  char* fatbuf = (char*)malloc(nfatblocks * BSIZE);
  int* int_fatbuf = (int*)fatbuf;
  for(int idx = 0; idx < fs_size; idx++){
    //for metadata block, allocate the entry with 0
    if(idx < freehead){
      int_fatbuf[idx] = -1;
    }
    else{
      int_fatbuf[idx] = idx + 1;
      //printf("int_fatbuf[%d]: %d\n", idx, int_fatbuf[idx]);
    }
  }
  //write the fat blocks
  for(i = 0; i < nfatblocks; i++){
    wsect(fatblock + i, (char*)fatbuf + i * BSIZE);
  }
  free(fatbuf);
}



int falloc(uint linkblock){
  //read the superblock and store to sb
  char* buf = (char*)malloc(BSIZE);
  rsect(1, buf);
  memmove(&sb, buf, sizeof(sb));
  free(buf);
  //check the freeblks count
  if(sb.freeblks == 0){
    //printf("no free block\n");
    return -1;
  }
  //allocate the fatbuf and read the fat blocks
  char* fatbuf = (char*)malloc(nfatblocks * BSIZE);
  //read the fat blocks
  for(int i = 0; i < nfatblocks; i++){
    rsect(sb.fatstart + i, (char*)(fatbuf + i * BSIZE));
  }
  //find the free block
  int freeblk = sb.freehead;
  int* int_fatbuf = (int*)fatbuf;
  sb.freehead = int_fatbuf[freeblk];
  sb.freeblks--;
  //link the block
  if(linkblock == 0){
    //it means that the requested block is the first block of the file
    int_fatbuf[freeblk] = 0;
  }
  else{
    int_fatbuf[linkblock] = freeblk;
    int_fatbuf[freeblk] = 0;
  }
  //write the fat blocks
  for(int i = 0; i < nfatblocks; i++){
    wsect(sb.fatstart + i, (char*)(fatbuf + i * BSIZE));
  }
  free(fatbuf);
  //save the super block
  buf = (char*)malloc(BSIZE);
  memmove(buf, &sb, sizeof(sb));
  wsect(1, buf);
  free(buf);
  return freeblk;
}




int rfat(uint startblock){
  //read the fat blocks
  char* fatbuf = (char*)malloc(nfatblocks * BSIZE);
  //read the fat blocks
  for(int i = 0; i < nfatblocks; i++){
    rsect(sb.fatstart + i, (char*)(fatbuf + i * BSIZE));
  }
  // find the last block
  int lastblk = startblock;
  int* int_fatbuf = (int*)fatbuf;
  while(int_fatbuf[lastblk] != 0){
    lastblk = int_fatbuf[lastblk];
  }
  free(fatbuf);
  return lastblk;
}



void print_fatblock(){
  printf("freeheads: %d\n", sb.freehead);
  char* fatbuf = (char*)malloc(nfatblocks * BSIZE);
  //read the fat blocks
  for(int i = 0; i < nfatblocks; i++){
    rsect(sb.fatstart + i, (char*)(fatbuf + i * BSIZE));
  }
  for(int i = 0; i < nfatblocks-5; i++){
    int* fat = (int*)((char*)fatbuf + i * BSIZE);
    for(int j = i* (BSIZE/ sizeof(int)); j <i* (BSIZE/ sizeof(int)) +  BSIZE / sizeof(int); j++){
      printf("fat[%d]: %d\n", j, fat[j-i* (BSIZE/ sizeof(int))]);
    }
  }
  free(fatbuf);
}


void print_inodeblocks(){
  int ninodeblocks = 4;
  char* inodebuf = (char*)malloc(ninodeblocks * BSIZE);
  //read the inode blocks
  for(int i = 0; i < ninodeblocks; i++){
    rsect(sb.inodestart + i, (char*)(inodebuf + i * BSIZE));
  }
  for(int i = 0; i < ninodeblocks; i++){
    char* inodeblk = (char*)((char*)inodebuf + i * BSIZE);
    int offset = i*IPB;
    for(int j = 0; j < IPB; j++){
      struct dinode* din = (struct dinode*)(inodeblk + j * sizeof(struct dinode));
      int filesize = din->size;
      int num_datablks = (filesize + BSIZE - 1) / BSIZE;
      printf("inode[%d]: type %d, major %d, minor %d, nlink %d, size %d, datablks %d, startblk %d\n", offset + j, din->type, din->major, din->minor, din->nlink, din->size, num_datablks ,din->startblk);
    }
  }
  printf("sb.freehead: %d, sb.freeblks: %d, sb.magicnum: %d\n", sb.freehead, sb.freeblks, sb.magic);
}


void read_dirblocks(uint blknum){
  char* buf = (char*)malloc(BSIZE);
  rsect(blknum, buf);
  struct dirent* de = (struct dirent*)buf;
  for(int i = 0; i < BSIZE / sizeof(struct dirent); i++){
    printf("de[%d]: inum %d, name %s\n", i, de[i].inum, de[i].name);
  }
  free(buf);
}


void read_superblock(){
  char* buf = (char*)malloc(BSIZE);
  rsect(1, buf);
  struct superblock* sb = (struct superblock*)buf;
  printf("sb.magic: %d, sb.size: %d, sb.nblocks: %d, sb.ninodes: %d, sb.nlog: %d, sb.logstart: %d, sb.nfat: %d, sb.fatstart: %d, sb.freehead: %d, sb.freeblks: %d, sb.inodestart: %d\n", sb->magic, sb->size, sb->nblocks, sb->ninodes, sb->nlog, sb->logstart, sb->nfat, sb->fatstart, sb->freehead, sb->freeblks, sb->inodestart);
  free(buf);
}