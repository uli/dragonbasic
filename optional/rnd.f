{ -- MF/TIN random number generator
  -- Copyright (c) 2003 by Jeff Massung }

\ set the seed
:n seed ( x -- ) __rnd_seed ! ;

\ return a rando, number between min and max-1
: random ( min max -- n )
	\ rnd-(max-min)*(rnd/(max-min))+min
	over - rnd dup a! swap dup 7 swi * - + ;
