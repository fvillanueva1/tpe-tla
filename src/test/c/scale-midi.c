#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(int argc, char **argv) {
    assert(argc == 3);
    int gMajor = !strcmp(argv[2], "G");
    FILE *file = fopen(argv[1], "rb");
    assert(file);
    unsigned char data[100];
    size_t length = fread(data, 1, sizeof(data), file);
    assert(!ferror(file));
    assert(fclose(file) == 0);
    assert(length == 99);
    const unsigned char header[] = {'M','T','h','d',0,0,0,6,0,0,0,1,1,0xe0,'M','T','r','k',0,0,0,77};
    unsigned char tempo[] = {0,0xff,0x51,3,0x07,0xa1,0x20,0,0xc0,0};
    if (gMajor) tempo[9] = 40;
    const int cNotes[] = {60,62,64,65,67,69,71};
    const int gNotes[] = {67,69,71,72,74,76,78};
    const int *notes = gMajor ? gNotes : cNotes;
    assert(!memcmp(data, header, sizeof(header)));
    assert(!memcmp(data + 22, tempo, sizeof(tempo)));
    for (int i = 0; i < 7; ++i) {
        const unsigned char event[] = {0,0x90,(unsigned char)notes[i],64,0x83,0x60,0x80,(unsigned char)notes[i],0};
        assert(!memcmp(data + 32 + i * 9, event, sizeof(event)));
    }
    const unsigned char end[] = {0,0xff,0x2f,0};
    assert(!memcmp(data + 95, end, sizeof(end)));
    return 0;
}
