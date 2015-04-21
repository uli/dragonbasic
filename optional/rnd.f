{ -- MF/TIN random number generator
  -- Copyright (c) 2003 by Jeff Massung }

\ set the seed
:n seed ( x -- ) __rnd_seed ! ;

\ return a random number from 0-7FFF
:n rnd ( -- n ) 
	__rnd_seed @ 69069 # * 1+ dup __rnd_seed ! 17 # n/ ;

\ return a rando, number between min and max-1
: random ( min max -- n )
	\ rnd-(max-min)*(rnd/(max-min))+min
	over - rnd dup a! swap dup 7 swi * - + ;
