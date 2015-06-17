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
  int text = 0 , data = 0  , bss = 0 , fd = 0 ;
  unsigned char value = '4'        ; // 4;
  unsigned counter = 0             ;
  unsigned short addr = 0          ;
  int miss = 0, hit = 0            ;
  sim_database_t* vir_mem                    ;
  
  /* random number generator */
  //scanf ( "%d %d %d" , &text, &data, &bss ) ;
  //srand ( seed )         ;
  text = 20;
  data = 22;
  bss = 22;
  //total 64 pages -> 2048 bytes.
  
      
  /* initiating the virtual memory */
  vir_mem = vm_constructor( file, text, data, bss  ) ;
  vm_print(vir_mem);
  
  //int address[] = {204, 3000, 2500, 4000, 150, 1000, 550, 3159, 2941, 5021};

  for ( counter = 0 ; counter < ACCESS  ; counter++ )
    {
      int s ; 
      //addr = address[counter]  ; //rand();
      printf("Choose address to LOAD:\n");
      scanf("%hd", &addr);
      
      /*** DEBUG ***/
      if(addr == 5000)
      	break;
      else if(addr == 5001) {
      	printRAM();
      	continue;
      }
      else if(addr == 5002)
      	continue;
      /************/
      
      
      if ( ( s = vm_load( vir_mem, addr, &value ) ) == MISS )
	miss++ ;
      else if ( s == HIT )
	hit++ ; 
      
      //addr = rand () ; 
      printf("Choose address to STORE:\n");
      scanf("%hd", &addr);
      
      /*** DEBUG ***/
      if(addr == 5000)
      	break;
      else if(addr == 5001) {
      	printRAM();
      	continue;
      }
      else if(addr == 5002)
      	continue;
      /************/
      	
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


