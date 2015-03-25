OBJS = \
	wavy/wavy.gba \
	tut_1/tut_1.gba \
	tut_4/tut_4.gba \
	tut_6/tut_6.gba \
	tut_7/tut_7.gba \

all: $(OBJS)

clean:
	rm -f */*.gba

$(OBJS): %.gba: %.dbc
	cd $(@D); ../../dbc $(<F) $(@F)
