#ifndef TEST_H
#define TEST_H

char *test_printf(const char* fmt, ...);

int test_fail(const char *error);
int test_pass();
#define TEST(expr, error) (!(expr) ? test_fail(error) : test_pass())

#include <stdio.h>
int test_report_file(FILE *fp);

#endif /* TEST_H */
