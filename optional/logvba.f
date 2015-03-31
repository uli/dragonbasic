{ -- VBA logging functions
  -- Copyright (c) 2003 by Jeff Massung }

\ send a string to the VBA console
:n log ( a -- ) 1+ $ff swi drop ;
