#include <circle/util.h>


extern "C" void* memchr(const void* s, int c, size_t n);
extern "C" size_t strspn(const char* s, const char* accept);
extern "C" size_t strcspn(const char* s, const char* reject);
extern "C" char* strrchr(const char* s, int c);

void* memchr(const void* s, int c, size_t n)
{
    const unsigned char* p = (const unsigned char*)s;

    while (n--)
    {
        if (*p == (unsigned char)c)
            return (void*)p;
        p++;
    }

    return 0;
}

size_t strspn(const char* s, const char* accept)
{
    size_t i = 0;

    while (s[i])
    {
        const char* a = accept;
        int found = 0;

        while (*a)
        {
            if (s[i] == *a)
            {
                found = 1;
                break;
            }
            a++;
        }

        if (!found)
            break;

        i++;
    }

    return i;
}

size_t strcspn(const char* s, const char* reject)
{
    size_t i = 0;

    while (s[i])
    {
        const char* r = reject;

        while (*r)
        {
            if (s[i] == *r)
                return i;
            r++;
        }

        i++;
    }

    return i;
}

char* strrchr(const char* s, int c)
{
    const char* last = 0;

    while (*s)
    {
        if (*s == (char)c)
            last = s;
        s++;
    }

    return (char*)last;
}