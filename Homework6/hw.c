#include <stdio.h>
typedef struct Record
{
    unsigned char v1[3]; // Bob
    unsigned int v2;     // 76543210
} Record;
int main()
{
    unsigned char a[] = {'B', 'o', 'b', 0x76, 0x54, 0x32, 0x10, 0};
    Record *b = (Record *)a;
    printf("%s\n", b->v1);
    printf("%08x\n", b->v2);
}

// BobvT2
// 00103254