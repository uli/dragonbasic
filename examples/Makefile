
OBJS = wavy/wavy.gba

all: $(OBJS)

clean:
	rm -f */*.gba

$(OBJS): %.gba: %.dbc
	cd $(@D); ../../dbc $(<F) $(@F)
