#ifndef INCLUDED_CLIENT_H
#define INCLUDED_CLIENT_H

#ifdef __cplusplus
extern "C"
{
#endif

    int decm_auth(const char * ip, int port, const char * sn, const char * libver, const char mac[6], int timeout);

    int decm_log(const char * ip, int port, const char * sn, const char cpluuid[16], unsigned char opcmd, int timeout);

#ifdef __cplusplus
}
#endif

#endif
