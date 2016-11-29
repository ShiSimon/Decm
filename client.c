#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>

#include "client.h"
#include "aes.h"
#include "decm.h"

#ifdef _DEBUG
#define debug(format, ...) fprintf(stderr, format, ##__VA_ARGS__)
#else
#define debug(format, ...)
#endif

extern int setPrikey(const char * key, int keylen);

#define MAX_MSG_LEN 2064
static unsigned char msg[MAX_MSG_LEN];

static const uint8_t aes_key[16] = {0x54, 0x91, 0x83, 0x70, 0x67, 0x85, 0x4a, 0xdb, 0xb8, 0x61, 0x91, 0x52, 0x4d, 0xef, 0x98, 0x0c};
static const uint8_t aes_iv[16]  = {0xfc, 0xa4, 0xfa, 0x65, 0x43, 0x69, 0x4a, 0x74, 0xa2, 0xf9, 0x62, 0x69, 0xce, 0xd9, 0xd7, 0x95};

static int readSocket(int sockfd, unsigned char *data, unsigned int len, int timeout)
{
    int ret;
    int byte;
    fd_set rfds;
    struct timeval tv;
    FD_ZERO(&rfds);
    FD_SET(sockfd, &rfds);
    tv.tv_sec = timeout / 1000;
    tv.tv_usec = (timeout * 1000) % 1000000;
    ret = select(sockfd+1, &rfds, NULL, NULL, &tv);
    if (ret == -1)
    {
#ifdef _DEBUG
        perror("select");
#endif
        return -1;
    }
    else if(ret == 0)
    {
        //timeout
        return 0;
    }
        
    if(FD_ISSET(sockfd, &rfds))
    {
        if((byte=recv(sockfd, data, len, 0))==-1) 
        {
#ifdef _DEBUG
            perror("recv");
#endif
            return -2;
        }

        if(byte == 0)
        {
            debug("the socket closed\n");
            return -3;
        }

        return byte;
    }

    return -4;
}

static int readMsg(int sockfd, unsigned char msg[MAX_MSG_LEN], int timeout)
{
    int ret;
    int count = 0;
    int len = 0;
    int headlen = 6;
    int taillen = 3;

    while(1)
    {
        ret = readSocket(sockfd, msg+count, 1, timeout);
        if(ret==0)
        {
            return 0;
        }
        if(ret<0)
        {
            return -1;
        }
        if(msg[0] == 0x5A)
        {
            count += 1;
            break; //sync 0x5A
        }
        continue;
    }

    while(count < headlen)
    {
        ret = readSocket(sockfd, msg+count, headlen-count, timeout);
        if(ret==0)
        {
            return 0;
        }
        if(ret<0)
        {
            return -1;
        }
        count += ret;
    }

    len = (msg[2]<<8) + msg[3] +  headlen + taillen;
    if(len>MAX_MSG_LEN)
    {
        debug("the msg length(%d) is too long!\n", len);
        return -2;
    }
    
    while(count < len)
    {
        ret = readSocket(sockfd, msg+count, len-count, timeout);
        if(ret == 0)
        {
            return 0;
        }
        if(ret<0)
        {
            return -1;
        }
        count += ret;
    }

    return len;
}

static void HEXPrint(FILE * fp, const unsigned char *data, int len, const char *title)
{
#ifdef _DEBUG
    int i;
    time_t timep;
    struct tm *p;
    time(&timep);
    p=localtime(&timep);

    if( title ) fprintf(fp,"[%s]", title);
    
    fprintf(fp, "[%04d-%02d-%02d %02d:%02d:%02d]\n[", (1900+p->tm_year), (1+p->tm_mon), p->tm_mday, p->tm_hour, p->tm_min, p->tm_sec);
    for(i=0; i<len; i++)
    {
        fprintf(fp,"%02X ", data[i] & 0xFF );
        //if( (i+1) % 32 == 0 ) fprintf(fp,"\n");
    }
    fprintf(fp, "]\n");
    //if( len % 32 ) fprintf(fp,"\n");
    fflush(fp);
#endif
}

static unsigned short CalcCrc16(unsigned char * pData, int nLength)
{
    unsigned short cRc_16 = 0x0000;
    const unsigned short cnCRC_16 = 0x8005;
    unsigned long cRctable_16[256];
    unsigned short i,j,k;
 
    for (i=0,k=0;i<256;i++,k++)
    {
        cRc_16 = i<<8;
        for (j=8;j>0;j--)
        {
            if (cRc_16&0x8000)        
            {
                cRc_16 <<= 1;
                cRc_16 ^= cnCRC_16; 
            }
            else
                cRc_16<<=1;                   
        }
        cRctable_16[k] = cRc_16;
    }

    cRc_16 = 0x0000;
    while (nLength>0)
    {
        cRc_16 = (cRc_16 << 8) ^ cRctable_16[((cRc_16>>8) ^ *pData) & 0xff]; 
        nLength--;
        pData++;
    }
 
    return cRc_16;  
}

static int packMsg(unsigned char msg[MAX_MSG_LEN], unsigned char tag, unsigned char *data, int len)
{
    static unsigned char serial_number = 0;
    unsigned char body[2048];
    uint8_t iv[16];
    memcpy(iv, aes_iv, 16);

    if(len)
    {
        int i;
        struct AVAES aesc;
        unsigned char pad = 16 - (len % 16);
        memcpy(body, data, len);
        for(i=0; i<pad; i++)
        {
            body[len+i] = pad; //PKCS #7 填充
        }
        len += pad;
        av_aes_init(&aesc, aes_key, 128, 0);
        av_aes_crypt(&aesc, body, body, len/16, iv, 0);
    }


    msg[0] = 0x5A; //synctag
    msg[1] = tag;
    msg[2] = (len>>8) & 0xFF;
    msg[3] = len & 0xFF;
    msg[4] = 0x0;
    msg[5] = 0x0;
    memcpy(msg+6, body, len);       
    msg[len + 6] = serial_number++;
    msg[len + 7] = 0; //reserved
    msg[len + 8] = 0x77; //endcode
    
    unsigned short crc16 = CalcCrc16(msg, len + 9);
    *((unsigned short *)(msg + 4)) = crc16;
    return len + 9;
}

static int parseMsg(unsigned char *msg, int len, unsigned char tag, unsigned char data[2048])
{
    int dataLen = 0;
    unsigned short crc16;
    uint8_t iv[16];
    memcpy(iv, aes_iv, 16);
    
    memset(data, 0, 2048);
    
    if( msg[0] != 0x5A || msg[1] != tag ) return -1;
    
    dataLen = (msg[2]<<8) + msg[3];
    
    if( (dataLen + 9) != len ) return -2;

    crc16 = (msg[4]<<8) + msg[5];
    msg[4] = 0;
    msg[5] = 0;
    if(crc16 !=  CalcCrc16(msg, len))
    {
        debug("crc mismatch 0x%04x 0x%04x\n", crc16, CalcCrc16(msg, len));
        return -3;
    }
    msg[4] = (crc16>>8)&0xFF;
    msg[5] = crc16&0xFF;
    
    if( dataLen )
    {
        struct AVAES aesc;
        unsigned char pad;
        memcpy(data, msg + 6, dataLen);
        av_aes_init(&aesc, aes_key, 128, 1);
        av_aes_crypt(&aesc, data, data, dataLen/16, iv, 1);
        debug("BodyWithPadding Len: %d\n", dataLen);
        HEXPrint(stderr, data, dataLen, "Body(withpadding)");
        pad = data[dataLen-1];
        dataLen = dataLen - pad;
    }

    return dataLen;
}

static int decm_connect(int sockfd, const char * ip, int port, int timeout)
{
    int ret;
    int len;
    struct sockaddr_in address;
    int connect_ret = -1;;

    bzero(&address,sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    inet_pton(AF_INET,ip,&address.sin_addr);
    len = sizeof(address);

    {
        unsigned long ul = 1;
        ioctl(sockfd, FIONBIO, &ul);
    }
    if((ret = connect(sockfd, (struct sockaddr *)&address, len))==-1)
    {
        fd_set wfds;
        struct timeval tv;
        FD_ZERO(&wfds);
        FD_SET(sockfd, &wfds);
        tv.tv_sec = timeout / 1000;
        tv.tv_usec = (timeout * 1000) % 1000000;
        ret = select(sockfd+1, NULL, &wfds, NULL, &tv);
        if(ret > 0)
        {
            int error=-1;
            len = sizeof(int);
            getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, (socklen_t *)&len);
            if(error == 0)
                connect_ret = 0;
        }
    }
    else
    {
        connect_ret = 0;
    }
    {
        unsigned long ul = 0;
        ioctl(sockfd, FIONBIO, &ul);
    }

    return connect_ret;
}

int decm_auth(const char * ip, int port, const char * sn, const char * libver, const char mac[6], int timeout)
{
    int ret;
    int sockfd;
    int len;
    int byte;

    if(strlen(sn) > 15)
    {
        return -M_ERR_SOCKET_SN_TOO_LONG;
    }
   
    if((sockfd = socket(AF_INET,SOCK_STREAM,0)) == -1)
    {
#ifdef _DEBUG
        perror("socket");
#endif
        return -M_ERR_SOCKET_CREATE;
    }

    ret = decm_connect(sockfd, ip, port, timeout);
    if(ret < 0)
    {
        ret = -M_ERR_SOCKET_CONNECT;
        goto end__;
    }

    //send 0x00
    {
        unsigned char data[32];
        int msglen;
        memset(data, 0, 32);
        memcpy(data, sn, strlen(sn));
        memcpy(data+16, libver, strlen(libver));
        memcpy(data+24, mac, 6);
        msglen = packMsg(msg, 0x00, data, 32);
        HEXPrint(stderr, msg, msglen, "Send");
        HEXPrint(stderr, data, 32, "Body");
        if((byte=send(sockfd, msg, msglen,0))==-1)
        {
#ifdef _DEBUG
            perror("send");
#endif
            ret = -M_ERR_SOCKET_SEND;
            goto end__;
        }
    }

    //receive 0x01
    {
        time_t t_server;
        time_t t_local;
        int dataLen;
        unsigned char data[2048];
        ret = readMsg(sockfd, msg, timeout);
        if(ret==0 || ret<0)
        {
            ret = -M_ERR_SOCKET_READ;
            goto end__;
        }
        len = (msg[2]<<8) + msg[3];
        if(len + 9 != ret)
        {
            ret = -M_ERR_SOCKET_MSG_LEN;
            goto end__;
        }
        
        HEXPrint(stderr, msg, ret, "Recv");
        dataLen = parseMsg(msg, ret, 0x01, data);
        if(dataLen < 0)
        {
            ret = -M_ERR_SOCKET_MSG_PARSE;
            goto end__;
        }
        HEXPrint(stderr, data, dataLen, "Body");
        t_server = (time_t)((data[1]<<24) + (data[2]<<16) + (data[3]<<8) + data[4]);
        t_local = time(NULL);
        debug("AuthResult: %02x\n", data[0]);
        debug("local time %d\n", (int)t_local);
        debug("platform time %d\n", (int)t_server);

        if(data[0])
        {
            if(data[0] == 1)
            {
                ret =  -M_ERR_SOCKET_INVALID_SN;
            }
            else if(data[0] == 2)
            {
                ret =  -M_ERR_SOCKET_INVALID_SN;
            }
            else if(data[0] == 3)
            {
                ret =  -M_ERR_SOCKET_INVALID_VER;
            }
            else
            {
                ret = -M_ERR_SOCKET_INVALID_RESULT;
            }
            goto end__;
        }

        if(t_local < t_server)
        {
            if(t_server - t_local > 86400)
            {
                ret = -M_ERR_SOCKET_INVALID_LOCAL_TIME;
                goto end__;
            }
        }
        else if(t_local > t_server)
        {
            if(t_server - t_local > 86400)
            {
                ret = -M_ERR_SOCKET_INVALID_LOCAL_TIME;
                goto end__;
            }
        }
        if(data[0] == 0)
        {
            setPrikey((char *)(data+5), dataLen-5);
        }
    }

    ret = 0;

end__:
    close(sockfd);
    return ret;
}

int decm_log(const char * ip, int port, const char * sn, const char cpluuid[16], unsigned char opcmd, int timeout)
{
    int ret;
    int sockfd;
    int len;
    int byte;
   
    if((sockfd = socket(AF_INET,SOCK_STREAM,0)) == -1)
    {
#ifdef _DEBUG
        perror("socket");
#endif
        return -M_ERR_SOCKET_CREATE;
    }

    ret = decm_connect(sockfd, ip, port, timeout);
    if(ret < 0)
    {
        ret = -M_ERR_SOCKET_CONNECT;
        goto end__;
    }

    //send 0x02
    {
        unsigned char data[48];
        int msglen;
        int t = (int)time(NULL);
        memset(data, 0, 48);
        memcpy(data, sn, strlen(sn));
        memcpy(data+16, cpluuid, 16);
        data[32] = (t>>24)&0xFF;
        data[33] = (t>>16)&0xFF;
        data[34] = (t>>8)&0xFF;
        data[35] = t&0xFF;
        data[36] = opcmd;
        msglen = packMsg(msg, 0x02, data, 48);
        HEXPrint(stderr, msg, msglen, "Send");
        HEXPrint(stderr, data, 48, "Body");
        if((byte=send(sockfd, msg, msglen,0))==-1)
        {
#ifdef _DEBUG
            perror("send");
#endif
            ret = -M_ERR_SOCKET_SEND;
            goto end__;
        }
    }

    //receive 0x03
    {
        int ret;
        int dataLen;
        unsigned char data[2048];
        ret = readMsg(sockfd, msg, 1000);//1000ms
        if(ret==0 || ret<0)
        {
            ret = -M_ERR_SOCKET_READ;
            goto end__;
        }
        len = (msg[2]<<8) + msg[3];
        if(len + 9 != ret)
        {
            ret = -M_ERR_SOCKET_MSG_LEN;
            goto end__;
        }

        HEXPrint(stderr, msg, ret, "Recv");
        dataLen = parseMsg(msg, ret, 0x03, data);
        if(dataLen < 0)
        {
            ret = -M_ERR_SOCKET_MSG_PARSE;
            goto end__;
        }
        debug("BodyWithoutPadding Len: %d\n", dataLen);
        HEXPrint(stderr, data, dataLen, "Body");
        debug("ReportResult: %02x\n", data[0]);
    }

    ret = 0;

end__:
    close(sockfd);
    return ret;    
}
