
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h> 
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include "mem_sim.h" 

/**************************/
/* Name: Netanel Draiman  */
/*    ID: 200752152       */
/*       Home Ex5         */
/**************************/

/************************************************************/
/* SYSTEM Simulated:                                        */
/* 12 bit system - 12 bit per pointer                       */
/* virtual memory = 2^12 = 4096 bit                         */
/* 7 msb = page number - 2^7 = 128 pages                    */
/* 5 lsb = offset in page - size of page: 2^5 = 32 bit      */
/************************************************************/

//Sizes are in Bytes
#define SWAP_SIZE 4096
#define PAGE_SIZE 32
#define MEMORY_SIZE 512

#define FRAME_NUM MEMORY_SIZE/PAGE_SIZE //16 frames

/**********************************/
/********** Data Structs **********/
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
int exec_size; // save file size - used to determine beginning of heap\stack

static char RAM[MEMORY_SIZE]; //each 32 Bytes is a frame. total: 16 frames
int bitmap[FRAME_NUM]; //array to determine if RAM frame is occupied 
		       //1 = occupied, 0 = free.

char temp_page[PAGE_SIZE]; //temp page for swapping

int global_counter=0; //used for LRU
int lru[FRAME_NUM]; //holds global counter for each page loaded in RAM
int lru_page[FRAME_NUM]; //holds page number for each page loaded in RAM

//Statistics Variables
int count_memoryAccess=0; //# of times vmload  & vmstore got called
int count_hits = 0; //# of access to pages already in memory
int count_miss = 0; //# of pagefaults - access to pages not in memory
int count_illegalAddr = 0; //# of illegal addresses
int count_read_only_err = 0; //# of writing attempts to a read-only section


/**************************************************/
/********** Private Methods Declarations **********/
/**************************************************/
static void freeDb(sim_database_t*); //free memory
static int freeFrame(sim_database_t*); //find free frame in RAM
static int swap(sim_database_t*, int); //Swap out frame out of RAM.
static void init_tempPage(); //initialize temp page to PAGE_SIZE zeroes (32)


/******************************************/
/********** Main Program Methods **********/
/******************************************/

/*************************/
/*      Constructor      */
/*************************/
sim_database_t* vm_constructor(char *executable, int text_size, int data_size,
			       int bss_size){
  
  if(executable == NULL)
    return NULL;
  
  //Init database
  sim_database_t* db = (sim_database_t*)malloc(sizeof(sim_database_t));
  if(db == NULL) {
    perror("ERROR: failed to allocate memory for sim_database\n");
    return NULL;
  }
  
  db->page_table = (page_descriptor_t*)malloc(
				    sizeof(page_descriptor_t)*(numOfPages));
  if(db->page_table == NULL) {
    perror("ERROR: failed to allocate memory for page_table\n");
    free(db);
    return NULL;
  }
  
  //open executable and swap files
  db->executable_name = executable;
  if( (db->executable_fd = open(db->executable_name, O_RDONLY, 0777)) < 0) {
    perror("ERROR: failed to open executable\n");
    free(db->page_table);
    free(db);
    return NULL;
  }
  
  db->swapfile_name = "swapfile.txt";
  if((db->swapfile_fd = open(db->swapfile_name, O_RDWR | O_CREAT | O_TRUNC,
								0777)) < 0) {
    perror("ERROR: failed to create swap file\n");
    free(db->page_table);
    free(db);
    return NULL;
  }

  //Setup Page Table
  int i;
  for(i=0; i < numOfPages; i++) {
    if(i < text_size)
      db->page_table[i].permission = 1; //READ ONLY - Executable text segment
    else
      db->page_table[i].permission = 0; //READ & WRITE
      
    db->page_table[i].valid = 0;
    db->page_table[i].dirty = 0;
    db->page_table[i].frame = -1;
    
  }
  
  //Init LRU and Memory arrays
  for(i=0; i < FRAME_NUM; i++) {
    bitmap[i] = 0;
    lru[i] = 0;
    lru_page[i] = 0;
  }
  
  for(i=0; i < MEMORY_SIZE; i++) {
    RAM[i] = 0;
  }
  
  exec_size = text_size + bss_size + data_size; //size in pages
  
  return db;
}
/********************** Constructor END ***************************/

/*************************/
/*       vm_load         */
/*************************/

int vm_load(sim_database_t *sim_db, unsigned short address, 
	    unsigned char *p_char) {
  
  int page, offset, frame;
  page = (address >> 5); //shift number 5 bits to get page number
  offset = (address & (PAGE_SIZE-1)); // mask 5 LSBs to get offset number 
  
  //update counters
  count_memoryAccess++;
  global_counter++;
  
  //Check Address legality
  if(page >= numOfPages || page < 0) {
    count_illegalAddr++;
    
    perror("ERROR: illegal address - page out of range\n");
    return -1;
  }
  else if(offset >= PAGE_SIZE || offset < 0) {
    count_illegalAddr++;
    
    perror("ERROR: illegal address - offset out of range\n");
    return -1;
  }
  
  int bytes;
  
  /********** Find Page **********/
  
  //if in RAM
  if (sim_db->page_table[page].valid) {
    count_hits++;
    
    frame = sim_db->page_table[page].frame;
    lru[frame] = global_counter;
    *p_char = RAM[(frame * PAGE_SIZE) + offset];
    return 0;
  }
  
  //if in SWAP
  else if (sim_db->page_table[page].dirty) {
    count_miss++;
    
    //Position file offset to Page position
    bytes = lseek(sim_db->swapfile_fd, (page * PAGE_SIZE), SEEK_SET);
    if(bytes < 0 || bytes != (page * PAGE_SIZE)) {
      perror("ERROR: vmload failed to access SWAP file\n");
      return -1;
    }
    frame = freeFrame(sim_db); //get Free frame in RAM
    if(frame < 0) {
      perror("ERROR: failed to swap out frame\n");
      return -1;
    }
    
    //Read Page
    bytes = read(sim_db->swapfile_fd, &RAM[frame * PAGE_SIZE], PAGE_SIZE);
    if(bytes < 0 || bytes != PAGE_SIZE) {
      perror("ERROR: failed to read from SWAP file\n");
      return -1;
    }
  }
  
  //if in DATA/BSS
  else if( page < exec_size ) {
    count_miss++;
    
    bytes = lseek(sim_db->executable_fd, (page * PAGE_SIZE), SEEK_SET);
    if(bytes < 0 || bytes != (page * PAGE_SIZE)) {
      perror("ERROR: vmload failed to access Exec file\n");
      return -1;
    }
    frame = freeFrame(sim_db);
    if(frame < 0) {
      perror("ERROR: failed to swap out frame\n");
      return -1;
    }
    
    bytes = read(sim_db->executable_fd, &RAM[frame * PAGE_SIZE], PAGE_SIZE);
    if(bytes < 0 || bytes != PAGE_SIZE) {
      perror("ERROR: failed to read from Exec file\n");
      return -1;
    }
  }
  
  else {
    count_miss++;
    
    perror("ERROR: Page not Initialized\n");
    return -1;
  }
  
  *p_char = RAM[(frame * PAGE_SIZE) + offset];
  
  lru[frame] = global_counter;
  lru_page[frame] = page;
  bitmap[frame] = 1;
  
  sim_db->page_table[page].valid = 1;
  sim_db->page_table[page].frame = frame;
  
  return 0;
}
/********************** vm_load END ***************************/

/*************************/
/*       vm_store        */
/*************************/ 

int vm_store(sim_database_t *sim_db, unsigned short address, 
	     unsigned char value) {
  
  int page, offset, frame;
  page = (address >> 5); //shift number 5 bits to get page number
  offset = (address & (PAGE_SIZE-1)); // mask 5 LSBs to get offset number 
  
  //update counters
  count_memoryAccess++;
  global_counter++;
  
  //Check Address legality
  if(page >= numOfPages || page < 0) {
    count_illegalAddr++;
    
    perror("ERROR: illegal address - page out of range\n");
    return -1;
  }
  else if(offset >= PAGE_SIZE || offset < 0) {
    count_illegalAddr++;
    
    perror("ERROR: illegal address - offset out of range\n");
    return -1;
  }
  //check if address is part of exec text segement - read-only
  else if(sim_db->page_table[page].permission) {
    count_read_only_err++;
    
    perror("ERROR: trying to write onto a read-only address\n");
    return -1;
  }
  
  int bytes;
  
  /********** Find Page **********/
  
  //if in RAM
  if(sim_db->page_table[page].valid) {
    count_hits++;
    
    lru[frame] = global_counter;
    
    frame = sim_db->page_table[page].frame;
    sim_db->page_table[page].dirty = 1;
    
    RAM[(frame *  PAGE_SIZE) + offset] = value;
    return 0;
  }
  
  //if in SWAP
  else if (sim_db->page_table[page].dirty) {
    count_miss++;
    
    bytes = lseek(sim_db->swapfile_fd, (page * PAGE_SIZE), SEEK_SET);
    if(bytes < 0 || bytes != (page * PAGE_SIZE)) {
      perror("ERROR: vmload failed to access SWAP file\n");
      return -1;
    }
    frame = freeFrame(sim_db);
    if(frame < 0) {
      perror("ERROR: failed to swap out frame\n");
      return -1;
    }
    
    bytes = read(sim_db->swapfile_fd, &RAM[frame * PAGE_SIZE], PAGE_SIZE);
    if(bytes < 0 || bytes != PAGE_SIZE) {
      perror("ERROR: failed to read from SWAP file\n");
      return -1;
    }
    
  }
  
  //if in DATA/BSS
  else if (!sim_db->page_table[page].permission && page < exec_size) {
    count_miss++;
    
    bytes = lseek(sim_db->executable_fd, (page * PAGE_SIZE), SEEK_SET);
    if(bytes < 0 || bytes != (page * PAGE_SIZE)) {
      perror("ERROR: vmload failed to access DATA/BSS file\n");
      return -1;
     }
    frame = freeFrame(sim_db);
    if(frame < 0) {
      perror("ERROR: failed to swap out frame\n");
      return -1;
    }
    
    bytes = read(sim_db->executable_fd, &RAM[frame * PAGE_SIZE], PAGE_SIZE);
    if(bytes < 0 || bytes != PAGE_SIZE) {
      perror("ERROR: failed to read from DATA/BSS file\n");
      return -1;
    }
    
  }
  
  //else init new page
  else {
    count_miss++;
    
    init_tempPage(); //reset Temp Page
    frame = freeFrame(sim_db);
    if(frame < 0) {
      perror("ERROR: failed to swap out frame\n");
      return -1;
    }
    
    strncpy(&RAM[frame * PAGE_SIZE], temp_page, PAGE_SIZE);
  }
  
  
  lru[frame] = global_counter;
  lru_page[frame] = page;
  bitmap[frame] = 1;
  
  sim_db->page_table[page].frame = frame;
  sim_db->page_table[page].dirty = 1;
  sim_db->page_table[page].valid = 1;
  
  RAM[(frame * PAGE_SIZE) + offset] = value;
  
  return 0; 
}
/********************** vm_store END ***************************/

/*************************/
/*      Destructor       */
/*************************/

void vm_destructor(sim_database_t *sim_db) {
  close(sim_db->swapfile_fd);
  close(sim_db->executable_fd);
  freeDb(sim_db);
}
/********************** Destructor END ***************************/


/*************************/
/*       vm_print        */
/*************************/

void vm_print(sim_database_t* sim_db) {
  if(sim_db == NULL)
    return;
  
  printf("\nPrinting Database...\n");
  
  int i;
  page_descriptor_t page;
  
  printf("*** Printing Page Table ***\n");
  for(i=0; i < numOfPages; i++) {
    page = sim_db->page_table[i];
    printf("Page #%d - valid = %d, permission = %d, dirty = %d, frame = %d \n",
	   i, page.valid, page.permission, page.dirty, page.frame);
  }
  
  printf("Swap File name = %s\n", sim_db->swapfile_name);
  printf("Swap File fd = %d \n", sim_db->swapfile_fd);
  printf("Executable File name = %s\n", sim_db->executable_name);
  
  printf("\n*** Printing RAM ***\n");
  for(i=0; i < MEMORY_SIZE; i++) {
    if(!(i % PAGE_SIZE))
      printf("\nframe #%d: ", i/PAGE_SIZE);
    printf("%c", RAM[i]);
  }
  printf("\n\n*** Printing Statistics ***\n");
  printf("Memory Access = %d, Hits = %d, Miss = %d\n", 
	 count_memoryAccess, count_hits, count_miss);
  printf("Illegal Address = %d, Write to READ-ONLY = %d\n", 
	 count_illegalAddr, count_read_only_err);
  
}
/********************** vm_print END ***************************/

/****************************************************/
/********** Private Methods Implementation **********/
/****************************************************/

//Free Memory Method - (used outside of ctor)
static void freeDb(sim_database_t *db) {
  free(db->page_table);
  free(db);
}
/****************************************/
/****************************************/

//Finds free Frame in RAM or frees an old one
//return value: on sucess - frame #, on failure returns -1
static int freeFrame(sim_database_t *db) {
  int i, page = 0, frame = 0;
  int min_counter = INT_MAX, min_lru;
  int bytes;
  
  //search for free Frame in RAM
  for(i=0; i < FRAME_NUM; i++) {
    if(!bitmap[i]) 
      return i;
    if(min_counter > lru[i]) {
      min_counter = lru[i];
      min_lru = i;
    } 
  }
  //swap out Frame according to minimum LRU counter
  if(swap(db,min_lru)) {
    return -1;
  }
  
  //return Free Frame
  return min_lru;
}
/****************************************/
/****************************************/

//Swap out frame out of RAM according to LRU counter
static int swap(sim_database_t* db, int frame) {
  int i, page, bytes;
  
  page = lru_page[frame];
  
  //if not valid - error!
  if(!db->page_table[page].valid) {
    perror("ERROR: trying to swap out a non-valid page\n");
    return -1;
  }
  
  //if dirty - in SWAP
  else if (db->page_table[page].dirty) {
    //Write with offset
    bytes = pwrite(db->swapfile_fd, &RAM[frame * PAGE_SIZE], 
		   PAGE_SIZE, (page * PAGE_SIZE));
    if(bytes != PAGE_SIZE) {
      perror("ERROR: Swapping frame failed - writing to swap failed\n");
      return -1;
    }
  }
  
  //else in Exec - no need to swap out
  
  //Update page table
  db->page_table[page].valid = 0;
  db->page_table[page].frame = -1;
  bitmap[frame] = 0;
  
  return 0;
}
/****************************************/
/****************************************/

//initialize temp page to PAGE_SIZE zeroes (32)
static void init_tempPage() {
  int i;
  for(i=0; i < PAGE_SIZE; i++) {
    temp_page[i] = '0';
  }
}

/****************************************************/
/****************************************************/
/****************************************************/

