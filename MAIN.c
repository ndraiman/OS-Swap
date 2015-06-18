/******************************************************************************
    File : main.c

    Description : testing the vm module 
******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <malloc.h>
#include "mem_sim.h"

#define ACCESS 2000
#define MISS -1
#define HIT 0
/******************************************************************************
    Function main :
******************************************************************************/

int main ()
{
  char file[ 30 ] = "program_file" ;
  int seed = 0 , text = 0 , data = 0  , bss = 0 , fd = 0 ;
  unsigned char value = '4'          ;
  unsigned counter = 0             ;
  unsigned short addr = 0          ;
  int miss = 0, hit = 0            ;
  sim_database_t* vir_mem                    ;
  
  /* random number generator */
  scanf ( "%d %d %d %d" , &seed, &text, &data, &bss ) ;
  srand ( seed )         ;
      
  /* initiating the virtual memory */
  vir_mem = vm_constructor( file, text, data, bss  ) ;
  vm_print(vir_mem);

  for ( counter = 0 ; counter < ACCESS  ; counter++ )
    {
      //DEBUG
      if(!(counter%100)) {
	//char t;	
	printRAM(); 
	printf("MAIN: value = %c\n", value);
	//scanf("%c", &t);
      }
      /*****************/
      int s ; 
      addr = rand ()  ;
      if ( ( s = vm_load( vir_mem, addr, &value ) ) == MISS )
	miss++ ;
      else if ( s == HIT )
	hit++ ; 
      
      addr = rand () ; 
      if ( ( s = vm_store( vir_mem, addr, value ) ) == MISS )
	miss++ ;
      else if ( s == HIT )
	hit++ ;
    }

  vm_print(vir_mem);

  vm_destructor ( vir_mem ) ;
  
  return ( 0 ) ; 
}

/******************************************************************************
  
******************************************************************************/

