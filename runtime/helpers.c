/* Implementation of the COMPARE word. */
int rt_compare(const char *a, const char *b)
{
    int size_a = *a++;
    int size_b = *b++;

    while (size_a && size_b) {
        if (*a < *b)
            return 1;
        else if (*a > *b)
            return -1;
        ++a; ++b;
        --size_a; --size_b;
    }

    if (size_b)
        return 1;
    else if (size_a)
        return -1;

    return 0;
}
