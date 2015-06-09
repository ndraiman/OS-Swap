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
#define VM_FRAME_SIZE 32
/******************************************************************************
    Function main :
******************************************************************************/

int main ()
{
  char file[ 30 ] = "program_file" ;
  int seed = 0 , text = 0 , data = 0  , bss = 0 , fd = 0 ;
  unsigned char value = 4          ;
  unsigned counter = 0             ;
  unsigned short addr = 0          ;
  int miss = 0, hit = 0            ;
  sim_database_t* vir_mem                    ;
  
  /* random number generator */
  scanf ( "%d %d %d %d" , &seed, &text, &data, &bss ) ;
  srand ( seed )         ;

  /* open the file for writing */
  if ( ( fd = open ( file , O_RDWR | O_TRUNC | O_CREAT , 0777 ) ) < 0 )
    {
      perror ( "Error: open" ) ;
      exit ( EXIT_FAILURE ) ;
    }

  /* writing to the file */
  for ( counter = 0 ; counter < VM_FRAME_SIZE * ( text + data + bss ) ; counter++ )
    {
      /* only digits are witten to the file */
      value = ( rand () % 10 ) + 48 ; 
      if ( ( write ( fd, &value, 1 ) < 0 ) )
	{
	  perror ( "Error: write" ) ;
	  exit ( EXIT_FAILURE  ) ;
	}
    }

  close ( fd ) ;

  /* initiating the virtual memory */
  vir_mem = vm_constructor( file, text, data, bss  ) ;
  
  for ( counter = 0 ; counter < ACCESS  ; counter++ )
    {
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

  vm_destructor ( vir_mem ) ;
  
  return ( 0 ) ; 
}

/******************************************************************************
  
******************************************************************************/

