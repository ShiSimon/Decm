#ifndef MXF_DEMUX_H
#define MXF_DEMUX_H

#ifdef __cplusplus
extern "C"
{
#endif

typedef uint8_t UID[16];
typedef int64_t offset_t;

typedef enum packet_type
{
    TRIPLET_PACKET,//encrypted triplet
    ESSENCE_PACKET,//essence element
}PACKET_TYPE;

typedef struct {
    UID uid;
    unsigned matching_len;
    int id;
} MXFCodecUL;

enum CodecID {
    CODEC_ID_NONE= 0x0,
    
    /* video codecs */
    CODEC_ID_MPEG2VIDEO, ///< preferred ID for MPEG-1/2 video decoding
    CODEC_ID_MPEG4,
    CODEC_ID_RAWVIDEO,
    CODEC_ID_DVVIDEO,
    CODEC_ID_JPEG2000,
    CODEC_ID_DNXHD,

    /* various PCM "codecs" */
    CODEC_ID_PCM_S16LE= 0x10000,
    CODEC_ID_PCM_S16BE,
    CODEC_ID_PCM_ALAW,
    CODEC_ID_PCM,

    /* audio codecs */
    CODEC_ID_MP2= 0x15000,
    CODEC_ID_MP3, ///< preferred ID for decoding MPEG audio layer 1, 2 or 3
    CODEC_ID_AAC,
    CODEC_ID_AC3,
    CODEC_ID_DTS,

    /* added codecs */
    CODEC_ID_H264 = 0x20000,
};

typedef struct
{
    PACKET_TYPE type;//包的类型

    UID key;
    offset_t offset;//key的offset
    uint64_t length;//value的length

    uint8_t ivec[16];//iv
    uint8_t aes_key[16];//key
    uint8_t checkbuf[16];//用来CHECK KEY

    offset_t plain_offset;//明流offset
    uint32_t plain_size;
    offset_t aes_offset;//密流offset
    uint32_t aes_size;

    uint32_t orig_size;//明流填充前的大小

    offset_t end;//KLV的END位置

    int channels;               /* 音频声道数 */
    int bits_per_sample;        /* 音频每个采样占用的bit位 */
    int sampling_rate;          /* 音频采样率 */

    int codecID;                /* enum CodecID类型 */
    UID descriptor;             /* ES描述子 */
    
    int found;                  /* if index found or not */
    uint32_t index;             /* for input */
    uint64_t pos;               /* for output */
} KLVPacket;

//retrurn:  -1:bytes_num超过8字节  0:成功
int mxf_read_header(int fd, KLVPacket *klv);

//return:  -2:文件结束  0:成功读取一个ES包 -1:bytes_num超过8字节 -3:triplet error
int mxf_read_packet(int fd, KLVPacket *klv);

//跳过一个packet
//return:  -2:文件结束  0:成功跳过 -1:bytes_num超过8字节
int mxf_skip_packet(int fd, KLVPacket *klv);

/* filename:mxf file path */
/* ret <0:error  =0:ok */
int mxf_get_index_position(const char *filename, uint32_t index, uint64_t *pos);

/* filename:mxf file path */
/* ret <0:error  =0:ok */
int mxf_get_position(const char *filename, uint64_t *pbody, uint64_t *pfooter);

#ifdef __cplusplus
}
#endif

#endif
