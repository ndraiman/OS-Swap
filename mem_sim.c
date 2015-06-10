
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h> 
#include <fcntl.h>
#include <unistd.h>
#include "mem_sim.h" 

/**************************/
/* Name: Netanel Draiman  */
/*    ID: 200752152       */
/*       Home Ex5         */
/**************************/

/************************************************************/
/* SYSTEM:                                                  */
/* 12 bit system - 12 bit per pointer                       */
/* virtual memory = 2^12 = 4096 bit                         */
/* 7 msb = page number - 2^7 = 128 pages                    */
/* 5 lsb = offset in page - size of page: 2^5 = 32 bit      */
/************************************************************/

//Sizes are in Bytes
#define SWAP_SIZE 4096
#define FRAME_SIZE 32
#define PAGE_SIZE 32
#define MEMORY_SIZE 512

#define FRAME_NUM MEMORY_SIZE/PAGE_SIZE

/**********************************/
/********** Data Strcuts **********/
/**********************************/
typedef struct page_descriptor {
  unsigned int valid; //valid bit - 1=in main memory, 0=in swapfile\executable
  unsigned int permission; // 0=READ & WRITE, 1=READ ONLY
  unsigned int dirty; // 1=data was written to page, 0=no data was written
  unsigned int frame; // page's frame # in main memory (if valid=1)
  
  //int ref_bit; //reference bit for LRU algorithm
  
}page_descriptor_t;

struct sim_database {
  page_descriptor_t* page_table; //pointer to page page_table
  char* swapfile_name; //name of swap file
  int swapfile_fd; //swap file fd
  char* executable_name; //name of executable file
  int executable_fd; //executable file fd
};

/**************************************/
/********** Global Variables **********/
/**************************************/
const int numOfPages = SWAP_SIZE/PAGE_SIZE; //128 pages
//const int numOfFrames = MEMORY_SIZE/PAGE_SIZE; //16 frames
int exec_size; // save file size - used to determine beginning of heap\stack in VM

static char* RAM[MEMORY_SIZE]; //RAM - each 32 Bytes is a frame. total: 16 frames
int bitmap[FRAME_NUM]; //array to determine if RAM frame is occupied - 1 = occupied, 0 = free.
int ref_bit[FRAME_NUM]; //LRU algorithm Implementation - reference bit for each frame 


//VM Tracking Variables
int memory_access=0;
int hits = 0;
int miss = 0;
int illegal_addr = 0;
int read_only_err = 0;



/**************************************************/
/********** Private Methods Declarations **********/
/**************************************************/
static void freeDb(sim_database_t*); //free memory
static int freeFrame(sim_database_t*); //find free frame in RAM


/******************************************/
/********** Main Program Methods **********/
/******************************************/
sim_database_t* vm_constructor(char *executable, int text_size, int data_size, int bss_size){
  
  if(executable == NULL)
    return NULL;
  
  sim_database_t* db = (sim_database_t*)malloc(sizeof(sim_database_t));
  if(db == NULL) {
    perror("ERROR: failed to allocate memory for sim_database\n");
    return NULL;
  }
  
  db->page_table = (page_descriptor_t*)malloc(sizeof(page_descriptor_t)*(numOfPages));
  if(db->page_table == NULL) {
    perror("ERROR: failed to allocate memory for page_table\n");
    free(db);
    return NULL;
  }
  
  
  db->executable_name = executable;
  if( (db->executable_fd = open(db->executable_name, O_RDONLY, 0777)) < 0) {
    perror("ERROR: failed to open executable\n");
    free(db->page_table);
    free(db);
    return NULL;
  }

  
  int i;
  for(i=0; i < numOfPages; i++) {
    if(i < text_size)
      db->page_table[i].permission = 1; //READ ONLY
    else
      db->page_table[i].permission = 0; //READ & WRITE
      
    db->page_table[i].valid = 0;
    db->page_table[i].dirty = 0;
    db->page_table[i].frame = -1;
  }
  
  db->swapfile_name = "swapfile";
  if( (db->swapfile_fd = open(db->swapfile_name, O_RDWR | O_CREAT, 0777)) < 0) {
    perror("ERROR: failed to create swap file\n");
    free(db->page_table);
    free(db);
    return NULL;
  }
  
  for(i=0; i < FRAME_NUM; i++) {
   bitmap[i] = 0;
   ref_bit[i] = 0;
  }
  
  exec_size = text_size + bss_size + data_size; //assign global variable - size in pages
  
  return db;
}



int vm_load(sim_database_t *sim_db, unsigned short address, unsigned char *p_char) {
  int page, offset, frame;
  page = address >> 5; //shift number 5 bits to get page number
  offset = address & (PAGE_SIZE-1); // mask 5 LSBs to get offset number 
  
  //Check Address legality
  if(page >= numOfPages || page < 0) {
    perror("ERROR: illegal address - page out of range\n");
    return -1;
  }
  else if(offset >= PAGE_SIZE || offset < 0) {
   perror("ERROR: illegal address - offset out of range\n");
   return -1;
  }
  
  //Find Data:
  //if in RAM
  if (sim_db->page_table[page].valid) { //!= 0 
    frame = sim_db->page_table[page].frame;
    ref_bit[frame] = 1; //frame was referenced, change ref_bit to 1.
    p_char = RAM[(frame * PAGE_SIZE) + offset];
    return 0;
  }
  
  
    
  
  /*** Issues ***/
  /*
   * how do i check if a memory cell is in heap or stack? --needed to check if heap address was initialized
   * how do i assign memory for heap & stack? --in ctor
   */
  
  
  
  
  return 0;
}

int vm_store(sim_database_t *sim_db, unsigned short address, unsigned char value) {
 return -1; 
}

void vm_destructor(sim_database_t *sim_db) {
  
}

void vm_print(sim_database_t* sim_db) {
  freeDb(sim_db);
}

/****************************************************/
/********** Private Methods Implementation **********/
/****************************************************/

//Free Memory Method - (used outside of ctor)
static void freeDb(sim_database_t *db) {
  free(db->page_table);
  free(db);
}

//Finds free Frame in RAM or frees an old one
//return value: on sucess - frame #, on failure returns -1
static int freeFrame(sim_database_t *db) {
  int i, frame = 0;
  
  //search for free Frame in RAM
  for(i=0; i < FRAME_NUM; i++) {
   if(!bitmap[i]) 
     return i;
  }
  
  //RAM is full
  //find frame with ref_bit = 0, if ref_bit = 1 - change to 0 to replace next time;
  for(i=0; i < FRAME_NUM; i++) {
    if(!ref_bit[i]) {
      frame = i;
      break;
    }
    else
      ref_bit[i/PAGE_SIZE] = 0;
  }
  if(db->page_table[frame].dirty) {
    int swap_loc = lseek(db->swapfile_fd, (frame * PAGE_SIZE), SEEK_SET);
    if(swap_loc < 0)
      return -1;
    int bytesWritten = write(db->swapfile_fd, RAM[frame], PAGE_SIZE);
    if(bytesWritten < 0)
      return -1;
    db->page_table[frame].dirty = 0;
  }
  return frame;
}



