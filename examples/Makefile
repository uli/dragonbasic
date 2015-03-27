OBJS = \
	breakout/breakout.gba \
	gapman/gapman2.gba \
	old_tut_4/old_tut_4.gba \
	tut_1/tut_1.gba \
	tut_2/tut_2.gba \
	tut_3/tut_3.gba \
	tut_4/tut_4.gba \
	tut_5/tut_5.gba \
	tut_6/tut_6.gba \
	tut_7/tut_7.gba \
	tut_8/tut_8.gba \
	tut_9/tut_9.gba \
	wavy/wavy.gba \

all: $(OBJS)

clean:
	rm -f */*.gba */*.sym

$(OBJS): %.gba: %.dbc
	cd $(@D); ../../dbc -sym $(<F) $(@F)
