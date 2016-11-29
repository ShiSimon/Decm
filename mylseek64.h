#ifndef _MY_LSEEK64
#define _MY_LSEEK64

#ifdef ARM_LINUX_ANDROIDEABI
off64_t lseek_64(int fd, off64_t offset, int whence);
int fseek_64(FILE *stream, off64_t offset, int whence);
off64_t ftell_64(FILE *stream);
#else
off_t lseek_64(int fd, off_t offset, int whence);
int fseek_64(FILE *stream, off_t offset, int whence);
off_t ftell_64(FILE *stream);
#endif

#endif
