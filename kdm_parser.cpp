#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdint.h>

#include "kdm_util.hpp"
#include "kdm_parser.h"

//#include "touying_cfg.h"

#define BUF_SIZE 4096

#define PRINT_KEY(s, x) fprintf(stderr, "%s %02hhX %02hhX %02hhX %02hhX %02hhX %02hhX %02hhX %02hhX %02hhX %02hhX %02hhX %02hhX %02hhX %02hhX %02hhX %02hhX\n", s, \
                                (x)[0], (x)[1], (x)[2], (x)[3], (x)[4], (x)[5], (x)[6], (x)[7], \
                                (x)[8], (x)[9], (x)[10], (x)[11], (x)[12], (x)[13], (x)[14], (x)[15])

/* return:
       0 成功得到key，档期正确; 
       -1:无法得到密钥;
       -2:档期未到;
       -3:档期已过 */
int find_kdm_info(KDM_INFO_T *pKdm, const char * kdmdir)
{
    DIR * dir;
    DIR * isdir;
    struct dirent * ptr;
    char tmp_name[512];
    int kdm_num;
    int flag = 0;
    int j=0;
    time_t now = time(NULL);

    if(kdmdir==NULL) kdmdir = KDMFULLDIR;
    dir= opendir(kdmdir);
    if(dir == NULL)
    {
        perror("open kdmdir");
        return -1;
    }
    
    while((ptr= readdir(dir))!= NULL)
    {
        if(strcmp(kdmdir, KDMFULLDIR) == 0)
        {
            if(!strcmp(ptr->d_name, ".")  ||
               !strcmp(ptr->d_name, "..")) continue;

            sprintf(tmp_name, "%s/%s", KDMFULLDIR, ptr->d_name);
            isdir = opendir(tmp_name);
            if(isdir == NULL)
            {
                remove(tmp_name);
                continue;
            }
            else
            {
                closedir(isdir);
            }
            fprintf(stderr, "[%s:%d]readdir name:[%s]\n", __FILE__, __LINE__, ptr->d_name);
            // 找指定的CPL UUID
            if(strcmp(ptr->d_name, (const char*)pKdm->cpluuid_))continue;
        }
        else
        {
            if(strcmp(ptr->d_name, ".")) continue;
            strcpy(tmp_name, kdmdir);
        }
        
        // 寻找此UUID符合的目录下各个KDM文件
        std::list<KdmParam> list_kdm;
        flag = KDMUTIL_DO_DEC | KDMUTIL_NO_DEC_FAIL | KDMUTIL_NO_CHECK_FAIL;
        fprintf(stderr, "scankdm:[%s]\n", tmp_name);
        scanKdms( tmp_name, list_kdm, flag, PRIKEY_FILE);
        kdm_num = list_kdm.size();
        fprintf(stderr, "scan num:%u\n", kdm_num);
        if(kdm_num == 0)
        {
            clearKdmsParamList( list_kdm);
            continue;
        }

        // get key num
        {
            std::list<KdmParam>::iterator iter, iter_end = list_kdm.end();
            for( iter = list_kdm.begin(); iter!= iter_end; ++iter)
            {
                if( iter->p_kdm_)
                {
                    std::list<KdmDecKey> list_dec = iter->p_kdm_->GetDecKeyList();
                    pKdm->key_num_ += list_dec.size();
                }
            }
        }
        
        pKdm->pKey_ = (struct key_info*)malloc(pKdm->key_num_ * sizeof(struct key_info));
        //fprintf(stderr, "%s:kdm key num:%d\n", __FUNCTION__, pKdm->key_num_);
                                        
        std::list<KdmParam>::iterator iter, iter_end = list_kdm.end();
        for( iter = list_kdm.begin(); iter!= iter_end; ++iter)
        {
            if( iter->p_kdm_)
            {
                //fprintf( stderr, "file=\"%s\"\n", iter->path_);

                std::list<KdmDecKey> list_dec = iter->p_kdm_->GetDecKeyList();
                std::list<KdmDecKey>::iterator iter_dec, iter_end_dec = list_dec.end();
                int n;
                for( iter_dec = list_dec.begin(), n=0; iter_dec!=iter_end_dec; ++iter_dec, ++n)
                {
                    char kdm_key_uuid[45];
                                        
                    //fprintf( stderr, "dec_key[%d]\n", n);
                    PrintUuid(kdm_key_uuid, iter_dec->key_uuid_, UUID_NO_PREFIX);
                    //fprintf(stderr, "get kdm_key_uuid:[%s]\n", kdm_key_uuid);

                   //  if(strcmp((const char*)pKdm->in_keyuuid, (const char*)kdm_key_uuid))
//                     {
//                         fprintf(stderr, "keyuuid is different\n");
//                         continue;
//                     }
                    if(j<pKdm->key_num_)
                    {
                        strcpy(pKdm->pKey_[j].uuid_, kdm_key_uuid);
                        memcpy(pKdm->pKey_[j].aes_key_, iter_dec->aes_key_, 16);
                        if(pKdm->nv_before_ <= (unsigned long long)now &&
                           pKdm->nv_after_ >= (unsigned long long)now)
                        {
                            //fprintf(stderr, "do not copy nv time,because we can play now\n");
                        }
                        else
                        {
                            pKdm->nv_before_ = iter_dec->nv_before_;
                            pKdm->nv_after_ = iter_dec->nv_after_;
                        }
                        j++;
                    }
                    else
                    {
                        fprintf(stderr, "key num is not right\n");
                    }
                                                                                                    
                 //    fprintf( stdout, "    aes_key=");
//                     for(int i=0;i<16;i++) fprintf(stdout, "%02X", pKdm->pKey_[j-1].aes_key_[i]);
//                     fprintf( stdout, "\n");
                }// end for( iter_dec = list_dec_.begin()    
                
            }//end if( iter->p_kdm_)
        }//end for( iter = list_kdm.begin();
        clearKdmsParamList( list_kdm);
       
    }//end while((ptr= readdir(dir))!= NULL)

    return 0;
}

/* return:
       0 成功得到key，档期正确; 
       -1:无法得到密钥;
       -2:档期未到;
       -3:档期已过 */
int find_thekdm_info(KDM_INFO_T *pKdm, const char * kdmfile)
{
    int kdm_num;
    int flag = 0;
    int j=0;
    time_t now = time(NULL);

    if(1)
    {
        // 寻找此UUID符合的目录下各个KDM文件
        std::list<KdmParam> list_kdm;
        flag = KDMUTIL_DO_DEC | KDMUTIL_NO_DEC_FAIL | KDMUTIL_NO_CHECK_FAIL;
        parseKdm(kdmfile, list_kdm, flag, PRIKEY_FILE);
        kdm_num = list_kdm.size();
        //fprintf(stderr, "kdm num:%u\n", kdm_num);
        if(kdm_num == 0)
        {
            clearKdmsParamList( list_kdm);
            return -1;
        }

        // get key num
        {
            std::list<KdmParam>::iterator iter, iter_end = list_kdm.end();
            for( iter = list_kdm.begin(); iter!= iter_end; ++iter)
            {
                if( iter->p_kdm_)
                {
                    std::list<KdmDecKey> list_dec = iter->p_kdm_->GetDecKeyList();
                    pKdm->key_num_ += list_dec.size();
                }
            }
        }
        
        pKdm->pKey_ = (struct key_info*)malloc(pKdm->key_num_ * sizeof(struct key_info));
        //fprintf(stderr, "%s:kdm key num:%d\n", __FUNCTION__, pKdm->key_num_);
                                        
        std::list<KdmParam>::iterator iter, iter_end = list_kdm.end();
        for( iter = list_kdm.begin(); iter!= iter_end; ++iter)
        {
            if( iter->p_kdm_)
            {
                //fprintf( stderr, "file=\"%s\"\n", iter->path_);

                std::list<KdmDecKey> list_dec = iter->p_kdm_->GetDecKeyList();
                std::list<KdmDecKey>::iterator iter_dec, iter_end_dec = list_dec.end();
                int n;
                for( iter_dec = list_dec.begin(), n=0; iter_dec!=iter_end_dec; ++iter_dec, ++n)
                {
                    char kdm_key_uuid[45];
                                        
                    //fprintf( stderr, "dec_key[%d]\n", n);
                    PrintUuid(kdm_key_uuid, iter_dec->key_uuid_, UUID_NO_PREFIX);
                    //fprintf(stderr, "get kdm_key_uuid:[%s]\n", kdm_key_uuid);

                    if(j<pKdm->key_num_)
                    {
                        strcpy(pKdm->pKey_[j].uuid_, kdm_key_uuid);
                        memcpy(pKdm->pKey_[j].aes_key_, iter_dec->aes_key_, 16);
                        if(pKdm->nv_before_ <= (unsigned long long)now &&
                           pKdm->nv_after_ >= (unsigned long long)now)
                        {
                            //fprintf(stderr, "do not copy nv time,because we can play now\n");
                        }
                        else
                        {
                            pKdm->nv_before_ = iter_dec->nv_before_;
                            pKdm->nv_after_ = iter_dec->nv_after_;
                        }
                        j++;
                    }
                    else
                    {
                        fprintf(stderr, "key num is not right\n");
                    }
                }// end for( iter_dec = list_dec_.begin()    
            }//end if( iter->p_kdm_)
        }//end for( iter = list_kdm.begin();
        clearKdmsParamList( list_kdm);
    }//end while((ptr= readdir(dir))!= NULL)

    return 0;
}
