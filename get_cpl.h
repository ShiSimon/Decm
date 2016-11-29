/*
** get_cpl.h
**
** Made by pfgu
** Login   <pfgu@pfgu.r52>
**
** Started on  Thu Aug 21 14:28:21 2008 pfgu
** Last update Thu Aug 21 14:28:21 2008 pfgu
*/

#ifndef   	GET_CPL_H_
#define   	GET_CPL_H_

#define ERR_CPL_NO_CPL_XML    1
#define ERR_CPL_NO_XML_FOUND  2
#define ERR_CPL_GLOB_ERR      3
#define ERR_CPL_BAD_CPL_ID    4
#define ERR_CPL_NILL_CPL_ID   5
#define ERR_CPL_NO_CPL_ID     6
#define ERR_CPL_LOAD_XML      7

#define ERR_ASSET_LOAD_XML    8
#define ERR_ASSET_NO_ASSET    9

#define ERR_ASSET_NO_PKL_FN      10
#define ERR_ASSET_NO_PKL_FS      11
#define ERR_ASSET_NO_PKL_UUID    12
#define ERR_ASSET_NO_CPL_FN      13
#define ERR_ASSET_NO_CPL_FS      14
#define ERR_ASSET_NO_CPL_UUID    15
#define ERR_ASSET_NO_AUDIO_FN    16
#define ERR_ASSET_NO_AUDIO_FS    17
#define ERR_ASSET_NO_AUDIO_UUID  18
#define ERR_ASSET_NO_VIDEO_FN    19
#define ERR_ASSET_NO_VIDEO_FS    20
#define ERR_ASSET_NO_VIDEO_UUID  21

#define ERR_PKL_NO_ASSET  22
#define ERR_PKL_LOAD_XML  23 

#define ERR_ASSET_NOT_PKL    24

#define MAX_ASSETMAP_NUM    512       /* 最大允许的ASSETMAP数量 */
#define MAX_XML_SIZE       5*1024*1024   /* 5M 以上的XML文件不去打开 */


/* uuid:不带前缀36字节，前缀9字节 */

#ifdef __cplusplus
extern "C"
{
#endif

    struct Rational
    {
        int Numerator;
        int Denominator;
    };
    
    struct Reel
    {
        char *v_path_;          /* 动态分配 */
        char v_uuid_[64];
        struct Rational v_editrate_;
        unsigned long v_intrinsic_duration_;
        unsigned long v_entrypoint_;
        unsigned long v_duration_;
        struct Rational v_framerate_;
        int is_stereoscopic_;   /* 是否是3D */
        char v_keyid_[64];
        
        char *a_path_;          /* 动态分配 */
        char a_uuid_[64];
        struct Rational a_editrate_;
        unsigned long a_intrinsic_duration_;
        unsigned long a_entrypoint_;
        unsigned long a_duration_;
        char a_keyid_[64];
    };
        
    typedef struct cpl_info
    {
        char uuid_[64];         /* cpl uuid */
        char content_title_[64]; /* 电影名 */
        char content_kind_[32];
    
        struct Reel *p_reel_;   /* reel结构题数组 需要动态分配*/
        unsigned int reel_num_; /* 有几个reel 一次性分配*/
        
        char *assetmap_path_;   /* 对应的ASSETMAP绝对路径 动态分配 */
        char *pkl_path_;        /* 对应的PKL绝对路径 动态分配 */
        char *path_;            /* 此CPL的绝对路径 动态分配 */
        int is_complete_;       /*  这个电影是否完整 */
    }cpl_info_t;
    
    enum
    {
        ASSET_UNKNOW=0,
        ASSET_PKL,
        ASSET_CPL,
        ASSET_AUDIO,
        ASSET_VIDEO,
        ASSET_SUB               /* 字幕 */
    };

    struct asset
    {
        char *path_;
        char uuid_[64];
        int type_;               /* ASSET TYPE */
        unsigned long long file_size_;
        int is_complete_;       /*  是否完整 */
        char pkl_uuid_[64];        /* 属于哪个PKL */
    };
    
    typedef struct assetmap_info
    {
        char *path_;            /* assetmap完整路径 */
        struct asset *p_asset_; /* asset结构数组 一次性动态分配*/
        unsigned int asset_num_;       /* 有几个asset */
        unsigned long long file_size_; /* assetmap的文件大小 */
    }assetmap_info_t;

    unsigned long long GetFileSize(const char * fn);

    /* 
     * 根据当前的ASSETMAP 分析CPL文件，作用：填充cpl_info_t结构体
     * p_info指向的内存在外面分配好
     * cpl_info_t 中的一些结构使用 malloc 分配内存，使用完后要调用
     * ClearCplInfo 来清理 cpl_info_t 结构中分配的内存。
     * @param[in]  index 需要处理的CPL文件在p_assetmap中的索引
     * @param[in]  p_assetmap 资产文件列表
     * @param[out] p_info 得到的CPL 信息
     * @return 0:成功 <0失败
     *
     */
    int GetCplInfo( int index, assetmap_info_t *p_assetmap ,cpl_info_t *p_info);

    /* 清理使用过的 cpl_info_t
     *
     * 经过 GetCplInfo 后的 cpl_info_t 结构必须使用本函数进行清理。
     * 释放一些动态分配的内存
     */
    int ClearCplInfo( cpl_info_t *p_info);

    
    /* 
       递归查找dir下所有的ASSETMAP返回找到的个数，没有则返回0
    */
    unsigned int FindAssetMapPath(const char *dir, char *path[MAX_ASSETMAP_NUM]);
    
    /*  
        释放分配的ASSETMAP路径信息
    */
    int ClearAssetMapPath(char *path[MAX_ASSETMAP_NUM]);
    
    
    /* 
     * 根据输入的ASSETMAP路径，获取所有的ASSET信息
     * p_assetmap指向的内存已经分配好
     * assetmap_info_t 中的一些结构使用 malloc 分配内存，使用完后要调用
     * ClearAssetMap 来清理结构中分配的内存。
     * 返回:0成功， <0失败
     */
    int GetAssetMap( const char *assetmap_path, assetmap_info_t *p_assetmap);
    int ClearAssetMap( assetmap_info_t *p_assetmap);
    
    /* s:出错所在的函数,err_ret:出错的返回值 */
    void PrintErrInfo(const char *s, int err_ret);
    
#ifdef __cplusplus
}
#endif

#endif 	    /* !GET_CPL_H_ */
