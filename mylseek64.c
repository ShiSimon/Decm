#include <stdio.h>
#include <unistd.h>

#include "mylseek64.h"

#ifdef ARM_LINUX_ANDROIDEABI

off64_t lseek_64(int fd, off64_t offset, int whence)
{
    return lseek64(fd, offset, whence);
}

int fseek_64(FILE *stream, off64_t offset, int whence)
{
    int fd;
    if (feof(stream)) {
        rewind(stream);
    }
    else {
        setbuf(stream, NULL); //清空fread的缓存
    }
    fd = fileno(stream);
    if (lseek64(fd, offset, whence) == -1) {
        return -1;
    }
    return 0;  
}

off64_t ftell_64(FILE *stream)
{
    int fd = fileno(stream);
    return lseek64(fd, 0, SEEK_CUR);
}
#else
//一般情况下如果_FILE_OFFSET_BITS为64,那么off_t就是64bit，但在ndk下无效，ndk特地提供了lseek64
off_t lseek_64(int fd, off_t offset, int whence)
{
    return lseek(fd, offset, whence);
}

int fseek_64(FILE *stream, off_t offset, int whence)
{
    return fseeko(stream, offset, whence);
}

off_t ftell_64(FILE *stream)
{
    return ftello(stream);
}
#endif
