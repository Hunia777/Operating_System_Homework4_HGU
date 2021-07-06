#include <stdio.h>
#include "rmalloc.h"

int 
main ()
{
	void *p1, *p2, *p3, *p4 ;

	rmprint() ;

	p1 = rmalloc(9000) ; 
	printf("rmalloc(9000):%p\n", p1) ; 
	rmprint() ;

	p2 = rmalloc(2500) ; 
	printf("rmalloc(2500):%p\n", p2) ; 
	rmprint() ;

	p3 = rmalloc(1000) ; 
	printf("rmalloc(1000):%p\n", p3) ; 
	rmprint() ;

	p4 = rmalloc(1000) ; 
	printf("rmalloc(1000):%p\n", p4) ; 
	
	printf("rmalloc(9000):%p\n", p1) ; 
	p1 = rrealloc(p1,10);
	printf("realloc 9000 to 1000:%p\n", p1) ; 
	rmprint() ;

}