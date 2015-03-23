#include "framework/test.h"
#include <stdio.h>

void test_mixer(void);

int main(int argc, char *argv[])
{
	test_mixer();
	return test_report_file(stdout);
}
