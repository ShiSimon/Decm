#ifndef KDM_PARSER_H
#define KDM_PARSER_H

#define KDMFULLDIR "/mnt/disk/kdm"
#define PRIKEY_FILE "prikey.enc"

#ifdef __cplusplus
extern "C"
{
#endif

struct key_info
{
    char uuid_[50]; /* no_prefix */
    unsigned char aes_key_[16];
};

typedef struct _tagKDMInfo
{
    struct key_info *pKey_;      /* 动态分配 */
    int key_num_;                /* 有几把密钥 */
    char cpluuid_[50]; /* no_prefix */
    unsigned long long nv_before_;
    unsigned long long nv_after_;
}KDM_INFO_T;
    
    /* return:
       0 成功得到key，档期正确; 
       -1:无法得到密钥;
       -2:档期未到;
       -3:档期已过 */
    int find_kdm_info(KDM_INFO_T *pKdm, const char * kdmdir);

    int find_thekdm_info(KDM_INFO_T *pKdm, const char * kdmfile);

#ifdef __cplusplus
}
#endif


#endif
