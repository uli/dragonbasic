CREATE BAR 10 CELLS RESERVE
: BAR[] ( i*x -- a ) 2 # N* BAR + ;

CREATE BAZ 10 CELLS RESERVE
: BAZ[] ( i*x -- a ) 2 # N* BAZ + ;


ENTRY START
PROGRAM" test/dim_const.gba"

