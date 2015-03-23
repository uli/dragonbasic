CPPFLAGS = -I.. -DASSERT_ENABLE

FRAMEWORK_SOURCES = framework/test.c
TEST_SOURCES = unit_test.c unit/test_mixer.c
TEST_DEPS_SOURCES = ../src/pimp_mixer.c ../src/pimp_mixer_portable.c

all: unit_test
run: unit_test
	./unit_test

unit_test: $(TEST_SOURCES) $(TEST_DEPS_SOURCES) $(FRAMEWORK_SOURCES)


