
OBJS = wavy/wavy.gba tut_1/tut_1.gba

all: $(OBJS)

clean:
	rm -f */*.gba

$(OBJS): %.gba: %.dbc
	cd $(@D); ../../dbc $(<F) $(@F)
