
//====================================================================
//Public header for the memory simulation module.
//====================================================================

/**
* metadata type for the memory simulation library. this type is opaqe -
* struct specification is subject to internal implementation.
*/
typedef struct sim_database sim_database_t;

/**
* initialize virtual system 
* executable - file name of executable 
* text_size - size of text segment (in pages) 
* data_size - size of data segment (in pages) 
* bss_size - size of bss segment (in pages) 
* return: pointer to main data structure
*/ 
sim_database_t* vm_constructor(char *executable, int text_size, int data_size, int bss_size);

/**
* load value from memory address 
* sim_db - pointer to main data structure 
* address - virtual address to load value from 
* p_char – on success, the char to which p_char points 
* will be changed to the character read.
* return: 0 if successful, -1 otherwise
*/ 
int vm_load(sim_database_t *sim_db, unsigned short address, unsigned char *p_char);

/**
* store value in memory address 
* sim_db - pointer to main data structure 
* address - virtual address to store value in 
* value - value to store 
* return: 0 if store was successful, -1 otherwise 
*/ 
int vm_store(sim_database_t *sim_db, unsigned short address, unsigned char value);

/**
* release all resources: memory, files, ... 
* sim_db - pointer to main data structure 
* return: none 
*/ 
void vm_destructor(sim_database_t *sim_db);

/**
* print information: d.s., main memory, statistics... 
* sim_db - pointer to main data structure 
* return: none 
*/ 
void vm_print(sim_database_t* sim_db);
