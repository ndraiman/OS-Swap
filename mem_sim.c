
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h> 
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
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

#define FRAME_NUM MEMORY_SIZE/PAGE_SIZE //16 frames

/**********************************/
/********** Data Strcuts **********/
/**********************************/
typedef struct page_descriptor {
  unsigned int valid; //valid bit - 1=in main memory, 0=in swapfile\executable
  unsigned int permission; // 0=READ & WRITE, 1=READ ONLY
  unsigned int dirty; // 1=data was written to page, 0=no data was written
  unsigned int frame; // page's frame # in main memory (if valid=1)
  
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
int exec_size; // save file size - used to determine beginning of heap\stack in VM

static char RAM[MEMORY_SIZE]; //RAM - each 32 Bytes is a frame. total: 16 frames
int bitmap[FRAME_NUM]; //array to determine if RAM frame is occupied - 1 = occupied, 0 = free.
int lru[FRAME_NUM]; //holds global counter for each page loaded in RAM
int lru_page[FRAME_NUM]; //holds page number for each page loaded in LRU

char temp_page[PAGE_SIZE]; //temp page for swapping

int global_counter=0; //used for LRU

//Statistics Variables
int count_memoryAccess=0; //# of times vmload  & vmstore got called
int count_hits = 0; //# of access to pages already in memory
int count_miss = 0; //# of pagefaults - access to pages no in memory
int count_illegalAddr = 0; //# of illegal addresses
int count_read_only_err = 0; //# of writing attempts to a read-only section

/************************************************** 
Instructions:

in the exercise DISK refers to the Executable
any address that isnt part of Executable is part of the Stack\Heap

new page for Stack\Heap needs to be a new page filled with 32 zeros (use temp page)

if any change occured dirty=1 - will always remain as 1 !!!!
if dirty=1 it means we have to load the page from the SWAP! 
and not from exec
for example - pages loaded from DATA\BSS sections of exec that were changed 
will be saved to the SWAP

Determine Page Location:
Valid = 1 - page in RAM
Valid = 0 & Dirty = 1 - page in SWAP
Valid = 0 & Dirty = 0 - page in DISK (EXEC)


LRU - counter Implementation - save counter for each page
*/

/**************************************************/
/********** Private Methods Declarations **********/
/**************************************************/
static void freeDb(sim_database_t*); //free memory
static int freeFrame(sim_database_t*); //find free frame in RAM
static void init_tempPage(); //initialize temp page to PAGE_SIZE zeroes (32)

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
    lru[i] = 0;
  }
  
  exec_size = text_size + bss_size + data_size; //assign global variable - size in pages
  
  return db;
}



int vm_load(sim_database_t *sim_db, unsigned short address, unsigned char *p_char) {
  int page, offset, frame;
  page = (address >> 5); //shift number 5 bits to get page number
  offset = (address & (PAGE_SIZE-1)); // mask 5 LSBs to get offset number 
  
  //update counters
  count_memoryAccess++;
  global_counter++;
  
  //fprintf(stderr, "address = %d ,page = %d, numOfPages = %d \n", address, page, numOfPages); //debug
  
  //Check Address legality
  if(page >= numOfPages || page < 0) {
    perror("ERROR: illegal address - page out of range\n");
    count_illegalAddr++;
    return -1;
  }
  else if(offset >= PAGE_SIZE || offset < 0) {
   perror("ERROR: illegal address - offset out of range\n");
   count_illegalAddr++;
   return -1;
  }
  
  int bytes;
  
  /********** Find Page **********/
  
  //if in RAM
  if (sim_db->page_table[page].valid) { //!= 0 
    frame = sim_db->page_table[page].frame;
    lru[frame] = global_counter;
    p_char = &RAM[(frame * PAGE_SIZE) + offset];
    count_hits++;
    return 0;
  }
  //if in  SWAP
  else if (sim_db->page_table[page].dirty) {
    bytes = lseek(sim_db->swapfile_fd, (page * PAGE_SIZE), SEEK_SET);
    if(bytes < 0 || bytes != (page * PAGE_SIZE)) {
      perror("ERROR: vmload failed to access SWAP file\n");
      return -1;
    }
    frame = freeFrame(sim_db);
    if(frame < 0)
      return -1;
    
    bytes = read(sim_db->swapfile_fd, &RAM[frame], FRAME_SIZE);
    if(bytes < 0 || bytes != FRAME_SIZE) {
      perror("ERROR: failed to read from SWAP file\n");
      return -1;
    }
  }
  //else in executable
  else {
    bytes = lseek(sim_db->executable_fd, (page * PAGE_SIZE), SEEK_SET);
    if(bytes < 0 || bytes != (page * PAGE_SIZE)) {
      perror("ERROR: vmload failed to access SWAP file\n");
      return -1;
    }
    frame = freeFrame(sim_db);
    if(frame < 0)
      return -1;
    
    bytes = read(sim_db->executable_fd, &RAM[frame], FRAME_SIZE);
    if(bytes < 0 || bytes != FRAME_SIZE) {
      perror("ERROR: failed to read from SWAP file\n");
      return -1;
    }
  }
  
  sim_db->page_table[page].frame = frame;
  count_miss++;
  return 0;
}

int vm_store(sim_database_t *sim_db, unsigned short address, unsigned char value) {
  int page, offset, frame;
  page = (address >> 5); //shift number 5 bits to get page number
  offset = (address & (PAGE_SIZE-1)); // mask 5 LSBs to get offset number 
  
  //update counters
  count_memoryAccess++;
  global_counter++;
  
  //Check Address legality
  if(page >= numOfPages || page < 0) {
    perror("ERROR: illegal address - page out of range\n");
    count_illegalAddr++;
    return -1;
  }
  else if(offset >= PAGE_SIZE || offset < 0) {
   perror("ERROR: illegal address - offset out of range\n");
   count_illegalAddr++;
   return -1;
  }
  //check if address is part of exec text segement - read-only
  else if(sim_db->page_table[page].permission) {
    perror("ERROR: trying to write onto a read-only address\n");
    count_read_only_err++;
    return -1;
  }
  
  if(sim_db->page_table[page].valid) {
    frame = sim_db->page_table[page].frame;
    lru[frame] = global_counter;
    sim_db->page_table[page].dirty = 1;
    RAM[frame + offset] = value; //correct position?
    count_hits++;
    return 0;
  }
  
  
  /*** Solution ***/
  // RAM is char* - static char RAM[MEMORY_SIZE];
  // used &RAM[frame] to read()\write() from\to SWAP
  /*************************/
  
  return -1; 
}

void vm_destructor(sim_database_t *sim_db) {
  freeDb(sim_db);
}

void vm_print(sim_database_t* sim_db) {
  //print d.s.?? whats that?
  //statistics = counters? hits\miss etc
  
  //printf("%s \n", RAM);
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
  int i, page = 0, frame = 0;
  int min = INT_MAX, min_lru;
  int bytes;
  
  //search for free Frame in RAM
  for(i=0; i < FRAME_NUM; i++) {
    if(!bitmap[i]) 
      return i;
    if(min > lru[i]) {
      min = lru[i];
      min_lru = i;
    } 
  }

  //if reached here, means we have to SWAP-OUT frame at min_lru_index
  page = lru_page[min_lru];
  frame = db->page_table[page].frame;
  
  //page is READ-ONLY, no need to write into SWAP.
  if(db->page_table[page].permission)
    return frame;
  
  //Write page into SWAP
  bytes = lseek(db->swapfile_fd, (page * PAGE_SIZE), SEEK_SET);
  if(bytes < 0 || bytes != FRAME_SIZE) {
    perror("ERROR: failed to swapout frame into SWAP file\n");
    return -1;
  }
  bytes = write(db->swapfile_fd, &RAM[frame], FRAME_SIZE);
  if(bytes < 0 || bytes != FRAME_SIZE) {
    perror("ERROR: failed to swapout frame into SWAP file\n");
    return -1;
  }
  
  return frame;
}

//initialize temp page to PAGE_SIZE zeroes (32)
static void init_tempPage() {
  int i;
  for(i=0; i < PAGE_SIZE; i++) {
    temp_page[i] = '0';
  }
}


