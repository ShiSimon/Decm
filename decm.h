#ifndef INCLUDED_DECM_H
#define INCLUDED_DECM_H

#include <stdint.h>

#define MAX_PATH_LEN 1024

typedef unsigned int uint;
typedef unsigned char uchar;

/* 
   errno:返回值加上符号，比如return -M_ERR_NO_ASSETMAP
*/
enum Error
{
    M_SUCCESS = 0,
    M_ERR_UNKNOWN = 1,
    M_ERR_NO_ASSETMAP,
    M_ERR_NO_VOLINDEX,
    M_ERR_WRONG_ASSETMAP,
    M_ERR_NO_PKL,
    M_ERR_NO_CPL,
    M_ERR_PARSE_CPL,
    M_ERR_CPL_NOT_COMPLETE,
    M_ERR_OPEN_VIDEO_REEL,
    M_ERR_OPEN_AUDIO_REEL = 10,
    M_ERR_NO_KDMFILE,
    M_ERR_NO_RIGHT_KDM,
    M_ERR_KDM_NV_BEFORE,
    M_ERR_KDM_NV_AFTER,
    M_ERR_MALLOC,
    M_ERR_DCRY_WRONG_TYPE,
    M_ERR_DCRY_NO_AUDIO_KEYID,
    M_ERR_DCRY_NO_VIDEO_KEYID,
    M_ERR_ESSENCE_ELEMENT_KEY,
    M_ERR_ESSENCE_SIZE = 20,
    M_ERR_ENC_SIZE,
    M_ERR_AES_CHECK,
    M_ERR_SOCKET_CREATE,
    M_ERR_SOCKET_CONNECT,
    M_ERR_SOCKET_SEND,
    M_ERR_SOCKET_READ,
    M_ERR_SOCKET_SN_TOO_LONG,
    M_ERR_SOCKET_MSG_LEN,
    M_ERR_SOCKET_MSG_PARSE,
    M_ERR_SOCKET_INVALID_SN = 30,
    M_ERR_SOCKET_INVALID_VER,
    M_ERR_SOCKET_INVALID_RESULT,
    M_ERR_SOCKET_INVALID_LOCAL_TIME,
    M_ERR_SERVER_INFO,
    M_ERR_PREPARE,
    M_ERR_MAX_INSTANCE,
};

enum Codec
{
    CODEC_MPEG2VIDEO = 0,
    CODEC_H264 = 1,
    CODEC_PCM = 3,
    CODEC_AC3 = 4,
    CODEC_UNKNOWN = 999,
};

enum DataType
{
    AUDIO = 0,
    VIDEO = 1,
};

typedef struct
{
    char Assetmap[MAX_PATH_LEN];
    char CPL[MAX_PATH_LEN];
    char KDM[MAX_PATH_LEN];
    char PKL[MAX_PATH_LEN];
    char VolumeIndex[MAX_PATH_LEN];
} res_path_t;

typedef struct
{
    uint   EntryPoint;          //入口时间，其类型定义待定
    uint   duration;            //持续时间
    uint   IntrinsicDuration;   //内在持续时间
    uint   EditRateNum;         //编辑速率分子
    uint   EditRateDen;         //编辑速率分母
    char  *mxf_name;            //相应mxf的文件
    uint64_t   ofs_start;       //BODY起始位置,单位字节
    uint64_t   ofs_end;         //FOOTER起始位置，单位字节，等于BODY结束位置+1
    unsigned char uuid[4][16];  //该类型资产所对应的KEY，可能出现多个KEY对应，如同时存在加密和不加密
    int    encryption[4];       //显示哪个uuid包为加密，0:不加密；1:加密
    int	   num_uuid;            //对应的有效UUID的数量，预留，应该只有1个，其他UUID非音视频数据
    int    format;              //0: MPEG-2; 1: H.264; 3: PCM; 4: AC3
    int    pcm_rate;            //仅音频有效，音频采样率,对视频则=0
    int    pcm_bits;            //仅音频有效, 采样位数，对视频则=0
    int    pcm_nchn;            //通道数，音频通道数，对视频则=1
} track_t;

typedef struct
{
    track_t video;              //相应的video资产信息
    track_t audio;              //相应的audio资产信息
} reel_t;

typedef struct
{
    reel_t  reels[128];         //按照播放顺序排列mxf播放表
    int   num_reels;            //数量
} dcp_para_t;

#ifdef __cplusplus
extern "C"
{
#endif
    
    /* 
      ip: 代理转发服务器IP，或者云端认证服务器公网IP
      port: 代理转发服务器端口，或者云端认证服务器端口
      mac: 本机6字节mac地址
      sn:  终端序列号
      timeout: 超时值，单位ms
     */
    int decm_prepare(const char * ip, int port, const char mac[6], const char * sn, int timeout);

    /*
      dcp_path: in, 电影 dcp 目录,特别是 Assetmap 所在目录。例如:/path/to/dcp/dir
      kdm_path: in, 密钥 kdm 路径， 比如/path/to/kdm.xml，如果是明流，则为NULL
    */
    int decm_open(void ** ppdecm, const char * dcp_path, const char * kdm_path);

    /*
      decm:     in, decm_open 返回的句柄
    */
    void decm_close(void *pdecm);

    /*
      decm:     in, decm_open 返回的句柄
      res_path: out, DCP 主要资源文件的全路径
    */
    int decm_get_resource_path(void *pdecm, res_path_t *res_path);

    /*
      decm:      in, decm_open 返回的句柄
      name:      out, 电影名
      name_len_max:in, 电影名缓冲的最大长度，比如输入为256，则name长度含结尾符'\0'不能超过256
    */
    int decm_get_name(void *pdecm, char* name, int name_len_max);

    /*
      decm:      in, decm_open 返回的句柄
      dcp_para:  out, 播放参数结构
    */
    int decm_get_paras(void *pdecm, dcp_para_t * dcp_para);

    /*
      idx:       in, 当前需解密的reel
    */
    void decm_set_current_reel(void *pdecm, int idx);

    /*
      decm:      in, decm_open 返回的句柄
      in:        in, 输入加密的数据
      in_len:    in, 数据长度
      out:       out,输出缓冲
      out_len:   out,解密后的数据长度
      out_len_max: in, out buffer 的最大长度
    */
    int decm_dcry(void *pdecm, uchar *in, int in_len, uchar *out, int *out_len, int out_len_max, int type);

    /*
      decm:      in, decm_open 返回的句柄
      current_reel: in, 第几个卷，从0开始
      index:     in, 第几帧
      vpos/apos: out,第几帧对应的音视频位置索引
      return:    0 成功获取 <0 获取失败
    */
    int decm_get_index_position(void *pdecm, int current_reel, int index, uint64_t *vpos, uint64_t *apos);
	
	/*
      decm:      in, decm_open 返回的句柄
      return:    库版本号
    */
    const char * decm_version(void);

#ifdef __cplusplus
}
#endif
    
#endif
