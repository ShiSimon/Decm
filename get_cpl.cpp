#include <stdio.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <utility>
#include <malloc.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <stdexcept>
#include <auto_ptr.h>
#include "get_cpl.h"
#include "tinyxml.h"

#define ERR_CHECK(code)                         \
    case code:                                  \
    fprintf(stderr, "%s:" #code "\n", s);       \
    break

#define ASSETMAP_ELEM "AssetMap"
#define PKL_ELEM "PackingList"
#define CPL_ELEM "CompositionPlaylist"
#define ID_ELEM  "Id"
#define ID_PREFIX "urn:uuid:"

#define ST_INFO  __FILE__,__LINE__

/*如果文件名为一个目录,返回值为0*/
unsigned long long GetFileSize(const char * fn)
{
    int fd;
    struct stat64  buf;
    DIR  * dir;
    /*测试是否目录*/
    dir= opendir(fn);
    if(dir)
    {
        closedir(dir);
        return 0;
    }

    fd= open(fn, O_RDONLY|O_LARGEFILE);
    if(fd== -1) 
    {
        return 0;
    }
    close (fd);
    stat64(fn, &buf);
    return buf.st_size;
}

int GetCplInfo( int index, assetmap_info_t *p_assetmap ,cpl_info_t *p_info)
{
    TiXmlElement *p_reel = 0;
    TiXmlText *p_xmltext = 0;
    int i=0,reel_num=0;
    
    if(p_info)
    {
        ClearCplInfo(p_info);
    }
    else
    {
        fprintf(stderr, "[%s:%d] p_info not alloc mem\n", ST_INFO);
        return -1;
    }

    if(GetFileSize(p_assetmap->p_asset_[index].path_) > MAX_XML_SIZE)
    {
        fprintf(stderr, "[%s:%d] %s is too big!!\n", ST_INFO, p_assetmap->p_asset_[index].path_);
        return -1;
    }
        
    try
    {
        std::auto_ptr<TiXmlDocument> cpl_doc(new TiXmlDocument(p_assetmap->p_asset_[index].path_));
        if(!(cpl_doc->LoadFile()))
        {
            fprintf( stderr, "[%s:%d]Load [%s] fail\n", ST_INFO, p_assetmap->p_asset_[index].path_);
            return -ERR_ASSET_LOAD_XML;
        }

        
        TiXmlHandle xml_handle(cpl_doc.get());
        // uuid
        strcpy(p_info->uuid_,p_assetmap->p_asset_[index].uuid_);
    
        // ContentTitleText
        p_xmltext = xml_handle.
            FirstChild(CPL_ELEM).
            FirstChild("ContentTitleText").
            FirstChild().
            Text();
        if(p_xmltext)
        {
            strncpy(p_info->content_title_, 
                    p_xmltext->Value(),
                    sizeof(p_info->content_title_));
        }

        // ContentKind
        p_xmltext = xml_handle.
            FirstChild(CPL_ELEM).
            FirstChild("ContentKind").
            FirstChild().
            Text();
        if(p_xmltext)
        {
            strncpy(p_info->content_kind_,  
                    p_xmltext->Value(),
                    sizeof(p_info->content_kind_));
        }
        // assetmap_path_
        p_info->assetmap_path_ = (char*)malloc(strlen(p_assetmap->path_)+1);
        if(p_info->assetmap_path_)
        {
            strcpy(p_info->assetmap_path_, p_assetmap->path_);
        }
    
        // pkl_path_
        {
            int pkl_index = -1;
            // 寻找ID匹配的pkl index
            for(i=0; (unsigned int)i<p_assetmap->asset_num_; i++)
            {
                if(strcasecmp(p_assetmap->p_asset_[index].pkl_uuid_,
                              p_assetmap->p_asset_[i].uuid_) == 0)
                {
                    pkl_index = i;
                    break;
                }
            }
            if((unsigned int)i == p_assetmap->asset_num_)
            {
                fprintf(stderr, "[%s:%d]no PKL found\n", ST_INFO);
            }
            if((unsigned int)i!=p_assetmap->asset_num_) // 找到
            {
                p_info->pkl_path_ = (char*)malloc(strlen(p_assetmap->p_asset_[pkl_index].path_)+1);
                if(p_info->pkl_path_)
                {
                    strcpy(p_info->pkl_path_, p_assetmap->p_asset_[pkl_index].path_);
                }
            }
        }

        // CPL path_
        p_info->path_ = (char*)malloc(strlen(p_assetmap->p_asset_[index].path_)+1);
        if(p_info->path_)
        {
            strcpy(p_info->path_, p_assetmap->p_asset_[index].path_);
        }

        // is_complete_
        // 先假设是完整的
        p_info->is_complete_ = 1;
    
        // 获取Reel个数
        p_reel = xml_handle.
            FirstChild( CPL_ELEM).
            FirstChild( "ReelList").
            FirstChild( "Reel").
            Element();
        if( p_reel == 0)
        {
            fprintf( stderr, "[%s:%d]Cannot find Reel\n", ST_INFO);
            ClearCplInfo(p_info);
            return -ERR_ASSET_NO_ASSET;
        }
    
        for(; p_reel != 0; p_reel= p_reel->NextSiblingElement("Reel"))
        {
            TiXmlHandle reel_handle(p_reel);
            TiXmlText *p_xml_text = 0;
            p_xml_text = reel_handle.
                FirstChild("Id").
                FirstChild().
                Text();
            if( p_xml_text)
            {
                reel_num++;
            }
        }

        // 分配数组
        p_info->p_reel_ = (struct Reel*)malloc(sizeof(struct Reel) * reel_num);
        if(p_info->p_reel_ == 0)
        {
            fprintf(stderr, "[%s:%d]malloc error\n", ST_INFO);
            ClearCplInfo(p_info);
            return -1;
        }
        memset(p_info->p_reel_, 0, sizeof(struct Reel)*reel_num);
        p_info->reel_num_ = reel_num;
    
        p_reel = xml_handle.
            FirstChild( CPL_ELEM).
            FirstChild( "ReelList").
            FirstChild( "Reel").
            Element();
        // 填充Reel内部数据
        for(i=0; p_reel != 0; p_reel= p_reel->NextSiblingElement("Reel"))
        {
            TiXmlHandle reel_handle(p_reel);
            TiXmlText *p_xml_text = 0;
            int j=0;
            const char *picture = "MainPicture";

            if(reel_handle.
               FirstChild("AssetList").
               FirstChild("MainPicture").Element())
            {
                p_info->p_reel_[i].is_stereoscopic_ = 0;
            }
            else if(reel_handle.
                    FirstChild("AssetList").
                    FirstChild("msp-cpl:MainStereoscopicPicture").Element())
            {
                p_info->p_reel_[i].is_stereoscopic_ = 1;
                picture = "msp-cpl:MainStereoscopicPicture";
                fprintf(stderr, "[%s:%d]it is a MainStereoscopicPicture\n", ST_INFO);
            }
            else if(reel_handle.
                    FirstChild("AssetList").
                    FirstChild("MainStereoscopicPicture").Element())
            {
                p_info->p_reel_[i].is_stereoscopic_ = 1;
                picture = "MainStereoscopicPicture";
                fprintf(stderr, "[%s:%d]it is a MainStereoscopicPicture\n", ST_INFO);
            }
                
            // MainPicture
            p_xml_text = reel_handle.
                FirstChild("AssetList").
                FirstChild(picture).
                FirstChild("Id").
                FirstChild().
                Text();
            if(p_xml_text)          // MainPicture存在
            {
                for(j=0; (unsigned int)j<p_assetmap->asset_num_; j++)
                {
                    if(strcasecmp(p_xml_text->Value()+strlen(ID_PREFIX),
                                  p_assetmap->p_asset_[j].uuid_) == 0)
                    {
                        // path
                        p_info->p_reel_[i].v_path_ = 
                            (char*)malloc(strlen(p_assetmap->p_asset_[j].path_)+1);
                        if(p_info->p_reel_[i].v_path_)
                        {
                            strcpy(p_info->p_reel_[i].v_path_, p_assetmap->p_asset_[j].path_);
                        }
                        // uuid
                        strcpy(p_info->p_reel_[i].v_uuid_, p_assetmap->p_asset_[j].uuid_);
                        // is complete
                        if(p_assetmap->p_asset_[j].is_complete_ == 0)
                        {
                            p_info->is_complete_ = 0;
                        }
                        break;
                    }
                }
                if((unsigned int)j == p_assetmap->asset_num_) // 找不到
                {
                    fprintf(stderr, "[%s:%d]MainPicture not found\n", ST_INFO);
                    p_info->is_complete_ = 0;
                }
                else   // 找到
                {
                    // EditRate
                    p_xml_text = reel_handle.
                        FirstChild("AssetList").
                        FirstChild(picture).
                        FirstChild("EditRate").
                        FirstChild().
                        Text();
                    if(p_xml_text)
                    {
                        sscanf(p_xml_text->Value(), "%d %d",
                               &(p_info->p_reel_[i].v_editrate_.Numerator),
                               &(p_info->p_reel_[i].v_editrate_.Denominator));
                    }
                    // IntrinsicDuration
                    p_xml_text = reel_handle.
                        FirstChild("AssetList").
                        FirstChild(picture).
                        FirstChild("IntrinsicDuration").
                        FirstChild().
                        Text();
                    if(p_xml_text)
                    {
                        sscanf(p_xml_text->Value(), "%lu",
                               &(p_info->p_reel_[i].v_intrinsic_duration_));
                    }
                    // EntryPoint
                    p_xml_text = reel_handle.
                        FirstChild("AssetList").
                        FirstChild(picture).
                        FirstChild("EntryPoint").
                        FirstChild().
                        Text();
                    if(p_xml_text)
                    {
                        sscanf(p_xml_text->Value(), "%lu",
                               &(p_info->p_reel_[i].v_entrypoint_));
                    }
                    // Duration
                    p_xml_text = reel_handle.
                        FirstChild("AssetList").
                        FirstChild(picture).
                        FirstChild("Duration").
                        FirstChild().
                        Text();
                    if(p_xml_text)
                    {
                        sscanf(p_xml_text->Value(), "%lu",
                               &(p_info->p_reel_[i].v_duration_));
                    }
                    // keyid
                    p_xml_text = reel_handle.
                        FirstChild("AssetList").
                        FirstChild(picture).
                        FirstChild("KeyId").
                        FirstChild().
                        Text();
                    if(p_xml_text)
                    {
                        strncpy(p_info->p_reel_[i].v_keyid_,
                                p_xml_text->Value()+strlen(ID_PREFIX),
                                sizeof(p_info->p_reel_[i].v_keyid_));
                    }
                    // FrameRate
                    p_xml_text = reel_handle.
                        FirstChild("AssetList").
                        FirstChild(picture).
                        FirstChild("FrameRate").
                        FirstChild().
                        Text();
                    if(p_xml_text)
                    {
                        sscanf(p_xml_text->Value(), "%d %d",
                               &(p_info->p_reel_[i].v_framerate_.Numerator),
                               &(p_info->p_reel_[i].v_framerate_.Denominator));
                    }
                }
            
            }//end if(p_xml_text) 
        
            // MainSound
            p_xml_text = reel_handle.
                FirstChild("AssetList").
                FirstChild("MainSound").
                FirstChild("Id").
                FirstChild().
                Text();
            if(p_xml_text)          // MainSound存在
            {
                for(j=0; (unsigned int)j<p_assetmap->asset_num_; j++)
                {
                    if(strcasecmp(p_xml_text->Value()+strlen(ID_PREFIX),
                                  p_assetmap->p_asset_[j].uuid_) == 0)
                    {
                        // path
                        p_info->p_reel_[i].a_path_ = 
                            (char*)malloc(strlen(p_assetmap->p_asset_[j].path_)+1);
                        if(p_info->p_reel_[i].a_path_)
                        {
                            strcpy(p_info->p_reel_[i].a_path_, p_assetmap->p_asset_[j].path_);
                        }
                        // uuid
                        strcpy(p_info->p_reel_[i].a_uuid_, p_assetmap->p_asset_[j].uuid_);
                        // is complete
                        if(p_assetmap->p_asset_[j].is_complete_ == 0)
                        {
                            p_info->is_complete_ = 0;
                        }
                        break;
                    }
                }
                if((unsigned int)j == p_assetmap->asset_num_) // 找不到
                {
                    fprintf(stderr, "[%s:%d]MainSound not found\n", ST_INFO);
                    p_info->is_complete_ = 0;
                }
                else                        // 找到
                {
                    // EditRate
                    p_xml_text = reel_handle.
                        FirstChild("AssetList").
                        FirstChild("MainSound").
                        FirstChild("EditRate").
                        FirstChild().
                        Text();
                    if(p_xml_text)
                    {
                        sscanf(p_xml_text->Value(), "%d %d",
                               &(p_info->p_reel_[i].a_editrate_.Numerator),
                               &(p_info->p_reel_[i].a_editrate_.Denominator));
                    }
                    // IntrinsicDuration
                    p_xml_text = reel_handle.
                        FirstChild("AssetList").
                        FirstChild("MainSound").
                        FirstChild("IntrinsicDuration").
                        FirstChild().
                        Text();
                    if(p_xml_text)
                    {
                        sscanf(p_xml_text->Value(), "%lu",
                               &(p_info->p_reel_[i].a_intrinsic_duration_));
                    }
                    // EntryPoint
                    p_xml_text = reel_handle.
                        FirstChild("AssetList").
                        FirstChild("MainSound").
                        FirstChild("EntryPoint").
                        FirstChild().
                        Text();
                    if(p_xml_text)
                    {
                        sscanf(p_xml_text->Value(), "%lu",
                               &(p_info->p_reel_[i].a_entrypoint_));
                    }
                    // Duration
                    p_xml_text = reel_handle.
                        FirstChild("AssetList").
                        FirstChild("MainSound").
                        FirstChild("Duration").
                        FirstChild().
                        Text();
                    if(p_xml_text)
                    {
                        sscanf(p_xml_text->Value(), "%lu",
                               &(p_info->p_reel_[i].a_duration_));
                    }
                    // keyid
                    p_xml_text = reel_handle.
                        FirstChild("AssetList").
                        FirstChild("MainSound").
                        FirstChild("KeyId").
                        FirstChild().
                        Text();
                    if(p_xml_text)
                    {
                        strncpy(p_info->p_reel_[i].a_keyid_,
                                p_xml_text->Value()+strlen(ID_PREFIX),
                                sizeof(p_info->p_reel_[i].a_keyid_));
                    }
                }
            
            }//end if(p_xml_text) 

            i++;
        }//end for(i=0; p_reel != 0; p_re

    }
    catch( std::exception &e)
    {
        fprintf(stderr, "EXCEPT: {std} %s\n", e.what());
        return -1;
    }
    catch(...)
    {
        fprintf(stderr,  "EXCEPT: unknown\n");
        return -1;
    }
    return 0;
}

int ClearCplInfo( cpl_info_t *p_info)
{
    if(p_info == NULL)return 0;
    
    if(p_info->assetmap_path_)
    {
        free(p_info->assetmap_path_);
        p_info->assetmap_path_=0;
    }

    if(p_info->pkl_path_)
    {
        free(p_info->pkl_path_);
        p_info->pkl_path_=0;
    }

    if(p_info->path_)
    {
        free(p_info->path_);
        p_info->path_=0;
    }
        
    for(int i=0; (unsigned int)i<p_info->reel_num_ ;i++)
    {
        if(p_info->p_reel_[i].v_path_)
        {
            free(p_info->p_reel_[i].v_path_);
            p_info->p_reel_[i].v_path_ = NULL;
        }
        if(p_info->p_reel_[i].a_path_)
        {
            free(p_info->p_reel_[i].a_path_);
            p_info->p_reel_[i].a_path_ = NULL;
        }
    }
    
    if(p_info->p_reel_)
    {
        free(p_info->p_reel_);
        p_info->p_reel_ = 0;
    }
    memset(p_info, 0, sizeof(cpl_info_t));
    return 0;
}

unsigned int FindAssetMapPath(const char *dir, char *path[MAX_ASSETMAP_NUM])
{
    DIR *pdir = opendir( dir);
    if( pdir == 0)
    {
	return 0;
    }
    
    struct dirent *pdirent = readdir( pdir);
    while( pdirent)
    {

        if( strcmp(pdirent->d_name, "..") == 0
            || strcmp(pdirent->d_name, ".") == 0)
        {
            pdirent = readdir(pdir);
            continue;
        }
        
        struct stat64 buf64;
        std::string file_path(dir);
        file_path.append("/");
        file_path.append(pdirent->d_name);

        if((stat64(file_path.c_str(), &buf64)) < 0)
        {
            perror("stat64");
            pdirent = readdir(pdir);
            continue;
        }
        if(S_ISDIR(buf64.st_mode)) // 是目录
        {
            FindAssetMapPath(file_path.c_str(), path);
        }
        else                    // 是文件
        {
            if(!strcasecmp(pdirent->d_name, "ASSETMAP") ||
               !strcasecmp(pdirent->d_name, "ASSETMAP.xml"))
            {
                if(GetFileSize(file_path.c_str()) > MAX_XML_SIZE)
                {
                    fprintf(stderr, "[%s:%d] %s is too big!!\n", ST_INFO, file_path.c_str());
                    goto next__;
                }

                try
                {
                    // 判断是不是ASSETMAP格式
                    std::auto_ptr<TiXmlDocument> doc(new TiXmlDocument(file_path.c_str()));
                    if(!(doc->LoadFile()))
                    {
                        fprintf( stderr, "[%s:%d]Load [%s] fail\n", ST_INFO, file_path.c_str());
                        goto next__;
                    }
   
                    TiXmlHandle xml_handle(doc.get());
   
                    // 看是否是ASSETMAP XML
                    TiXmlText *p_id = xml_handle.
                        FirstChild(ASSETMAP_ELEM).
                        FirstChild(ID_ELEM).
                        FirstChild().
                        Text();
                    if( p_id == 0)
                    {
                        fprintf( stderr, "[%s:%d]: Load[%s], Element %s::%s not found\n",
                                 ST_INFO, file_path.c_str(), ASSETMAP_ELEM, ID_ELEM);
                        goto next__;
                    }
                    else            // 是ASSETMAP
                    {
                        int index=0,j=0;            // index 下个需要分配的指针索引
                    
                        for(j=0;j<MAX_ASSETMAP_NUM;j++)
                        {
                            if(path[j] == 0)break;
                        }
                        index = j;
                        path[index] = (char *)malloc(file_path.size()+1);
                        if(path[index]==NULL)
                        {
                            perror("malloc");
                            goto next__;
                        }
                        strcpy(path[index], file_path.c_str());
                    }
                }
                catch( std::exception &e)
                {
                    fprintf(stderr, "EXCEPT: {std} %s\n", e.what());
                    goto next__;
                }
                catch(...)
                {
                    fprintf(stderr,  "EXCEPT: unknown\n");
                    goto next__;
                }
            }//end  if(!strcasecmp(pdirent->d_name, "ASSETMAP")... 
        }// end  else // 是文件
    next__:
	pdirent = readdir(pdir);
    }//end while
    closedir(pdir);
    
    int n=0;
    for(n=0;n<MAX_ASSETMAP_NUM;n++)
    {
        if(path[n] == 0)break;
    }
    return n;
}


int ClearAssetMapPath(char *path[MAX_ASSETMAP_NUM])
{
    int num=0;
    
    for(int i=0;i<MAX_ASSETMAP_NUM;i++)
    {
        if(path[i])
        {
//            fprintf(stderr, "clear:[%p]\n", path[i]);
            free(path[i]);
            path[i] = 0;
            num++;
        }
    }
    fprintf(stderr, "[%s:%d]ClearAssetMapPath:clear num:%d\n", ST_INFO, num);
    return 0;
}

//将索引为index的PKL文件进行解析，并填充 pkl_uuid_,type_和is_complete_
static int ParsePklInfo(int index, assetmap_info_t *p_assetmap)
{
    
    TiXmlText *p_xml_text = 0;
    TiXmlElement *p_asset = 0;
    int i=0;
    
    if(index<0 || p_assetmap == 0)return -1;
    
    if(GetFileSize(p_assetmap->p_asset_[index].path_) > MAX_XML_SIZE)
    {
        fprintf(stderr, "[%s:%d] %s is too big!!\n", ST_INFO, p_assetmap->p_asset_[index].path_);
        return -1;
    }

    try
    {
        std::auto_ptr<TiXmlDocument> pkl_doc(new TiXmlDocument(p_assetmap->p_asset_[index].path_));
        if(!(pkl_doc->LoadFile()))
        {
            fprintf( stderr, "[%s:%d]Load [%s] fail\n", ST_INFO, p_assetmap->p_asset_[index].path_);
            return -ERR_ASSET_LOAD_XML;
        }
   
        TiXmlHandle pkl_xml_handle(pkl_doc.get());

        p_asset = pkl_xml_handle.
            FirstChild( PKL_ELEM).
            FirstChild( "AssetList").
            FirstChild( "Asset").
            Element();
        if( p_asset == 0)
        {
            fprintf( stderr, "[%s:%d]Cannot find PKL_ELEM::AssetList::Asset\n", ST_INFO);
            return -ERR_ASSET_NO_ASSET;
        }
    
        for( ; p_asset != 0; p_asset = p_asset->NextSiblingElement("Asset"))
        {
            TiXmlHandle asset_handle(p_asset);
            int asset_index = -1 ;
            char id[64];
                        
// 获取UUID
            p_xml_text = asset_handle.
                FirstChild("Id").
                FirstChild().
                Text();
            if( p_xml_text)
            {
                strcpy( id, p_xml_text->Value() + strlen(ID_PREFIX));
            }

            // 寻找ID匹配的asset index
            for(i=0; (unsigned int)i<p_assetmap->asset_num_; i++)
            {
                if(strcasecmp(id, p_assetmap->p_asset_[i].uuid_) == 0)
                {
                    asset_index = i;
                    break;
                }
            }
            if((unsigned int)i == p_assetmap->asset_num_)continue; // 没有找到

            // 填充类型
            p_xml_text = asset_handle.
                FirstChild( "Type").
                FirstChild().
                Text();
            if( p_xml_text)
            {
                std::string tmp = p_xml_text->Value();
                if(strstr(tmp.c_str(), "=CPL") || strcmp(tmp.c_str(), "text/xml") == 0)
                {
                    //fprintf(stderr, "[%s:%d]found CPL:%s\n", ST_INFO, p_assetmap->p_asset_[asset_index].path_);
                    p_assetmap->p_asset_[asset_index].type_ = ASSET_CPL;
                }
                else if(strstr(tmp.c_str(), "=Picture"))
                {
                    p_assetmap->p_asset_[asset_index].type_ = ASSET_VIDEO;
                }
                else if(strstr(tmp.c_str(), "=Sound"))
                {
                    p_assetmap->p_asset_[asset_index].type_ = ASSET_AUDIO;
                }
                else
                {
                    p_assetmap->p_asset_[asset_index].type_ = ASSET_UNKNOW;
                }
            }
       
            // 判断文件大小
            p_xml_text = asset_handle.
                FirstChild("Size").
                FirstChild().
                Text();
            if( p_xml_text)
            {
                unsigned long long text_size = 0;
                sscanf(p_xml_text->Value(), "%llu", &text_size);
                if(p_assetmap->p_asset_[asset_index].file_size_ != text_size) // 不完整
                {
                    p_assetmap->p_asset_[asset_index].is_complete_ = 0;
                }
                else
                {
                    p_assetmap->p_asset_[asset_index].is_complete_ = 1;
                }
            }
            // 我属于这个PKL
            strcpy(p_assetmap->p_asset_[asset_index].pkl_uuid_, p_assetmap->p_asset_[index].uuid_);
        
        } // 解析PKL完毕 for( ; p_asset != 0; p_asset =...
    }
    catch( std::exception &e)
    {
        fprintf(stderr, "EXCEPT: {std} %s\n", e.what());
        return -1;
    }
    catch(...)
    {
        fprintf(stderr,  "EXCEPT: unknown\n");
        return -1;
    }
    return 0;
}

/* 
   根据输入的ASSETMAP路径，获取所有的ASSET信息
*/
int GetAssetMap( const char *assetmap_path, assetmap_info_t *p_assetmap)
{
    TiXmlElement *p_asset = 0;
    int asset_num = 0 ;
    int i=0;
    std::string dir_path(assetmap_path);
    
    // TODO: judge
    i = dir_path.rfind("/");
    int len = dir_path.size()-i;
    dir_path.erase(i, len);
    //fprintf(stderr, "[%s:%d]path:{%s} dir:{%s}\n", ST_INFO, assetmap_path, dir_path.c_str());
                    
    if(p_assetmap)
    {
        ClearAssetMap(p_assetmap);
    }
    else
    {
        fprintf(stderr, "[%s:%d] p_assetmap not alloc mem\n", ST_INFO);
        return -1;
    }
    
    if(GetFileSize(assetmap_path) > MAX_XML_SIZE)
    {
        fprintf(stderr, "[%s:%d] %s is too big!!\n", ST_INFO, assetmap_path);
        return -1;
    }

    try
    {
        std::auto_ptr<TiXmlDocument> doc(new TiXmlDocument(assetmap_path));
        if(!(doc->LoadFile()))
        {
            fprintf( stderr, "[%s:%d]Load [%s] fail\n", ST_INFO, assetmap_path);
            return -ERR_ASSET_LOAD_XML;
        }
        // 填充路径和大小信息
        p_assetmap->path_ = (char*)malloc(strlen(assetmap_path) + 1);
        strcpy(p_assetmap->path_, assetmap_path);
        p_assetmap->file_size_ = GetFileSize(p_assetmap->path_);
        
        TiXmlHandle xml_handle(doc.get());
        p_asset = xml_handle.
            FirstChild( ASSETMAP_ELEM).
            FirstChild( "AssetList").
            FirstChild( "Asset").
            Element();
        if( p_asset == 0)
        {
            fprintf( stderr, "[%s:%d]Cannot find AssetMap::AssetList::Asset\n", ST_INFO);
            return -ERR_ASSET_NO_ASSET;
        }
    
        // 先统计asset个数
        for(; p_asset != 0; p_asset= p_asset->NextSiblingElement("Asset"))
        {
            TiXmlHandle asset_handle(p_asset);
            TiXmlText *p_xml_text = 0;
            p_xml_text = asset_handle.
                FirstChild("Id").
                FirstChild().
                Text();
            if( p_xml_text)
            {
                asset_num++;
            }
        }

        // 分配数组
        p_assetmap->p_asset_ = (struct asset*)malloc(sizeof(struct asset) * asset_num);
        if(p_assetmap->p_asset_ == 0)
        {
            fprintf(stderr, "[%s:%d]malloc error\n", ST_INFO);
            ClearAssetMap(p_assetmap);
            return -1;
        }
        memset(p_assetmap->p_asset_, 0, sizeof(struct asset)*asset_num);
        p_assetmap->asset_num_ = asset_num;
    
        p_asset = xml_handle.
            FirstChild( "AssetMap").
            FirstChild( "AssetList").
            FirstChild( "Asset").
            Element();
        // 填充asset内部数据
        for(i=0; p_asset != 0; p_asset= p_asset->NextSiblingElement("Asset"))
        {
            TiXmlHandle asset_handle(p_asset);
            TiXmlText *p_xml_text = 0;
            // 获取UUID
            p_xml_text = asset_handle.
                FirstChild("Id").
                FirstChild().
                Text();
            if( p_xml_text)
            {
                strncpy(p_assetmap->p_asset_[i].uuid_, 
                        p_xml_text->Value()+strlen(ID_PREFIX),
                        sizeof(p_assetmap->p_asset_[i].uuid_));
            }
        
            p_xml_text = asset_handle.
                FirstChild("ChunkList").
                FirstChild("Chunk").
                FirstChild("Path").
                FirstChild().
                Text();
            if( p_xml_text)
            {
                // path
                std::string tmp(dir_path);
                tmp.append("/");
                tmp.append(p_xml_text->Value());

                p_assetmap->p_asset_[i].path_ = (char*)malloc(tmp.size() + 1);
                if(p_assetmap->p_asset_[i].path_ == 0)
                {
                    fprintf(stderr, "[%s:%d]malloc error\n", ST_INFO);
                    ClearAssetMap(p_assetmap);
                    return -1;
                }
                strcpy(p_assetmap->p_asset_[i].path_, tmp.c_str());
                // filesize
                p_assetmap->p_asset_[i].file_size_ = GetFileSize(p_assetmap->p_asset_[i].path_);
            }

            if(p_assetmap->p_asset_[i].file_size_ == 0) //  不完整
            {
                p_assetmap->p_asset_[i].is_complete_ = 0;
                i++;
                continue;
            }
        
            // 寻找是否是PKL
            int tmp_len = strlen(p_assetmap->p_asset_[i].path_);
            if(strcasecmp(p_assetmap->p_asset_[i].path_ + tmp_len-4, ".xml") == 0)
            {
                if(GetFileSize(p_assetmap->p_asset_[i].path_) > MAX_XML_SIZE)
                {
                    fprintf(stderr, "[%s:%d] %s is too big!!\n", ST_INFO, p_assetmap->p_asset_[i].path_);
                    i++;
                    continue;
                }

                std::auto_ptr<TiXmlDocument> pkl_doc(new TiXmlDocument(p_assetmap->p_asset_[i].path_));
                if(!(pkl_doc->LoadFile()))
                {
                    fprintf( stderr, "[%s:%d]Load [%s] fail\n", ST_INFO, p_assetmap->p_asset_[i].path_);
                    i++;
                    continue;
                }
   
                TiXmlHandle pkl_handle(pkl_doc.get());
   
                // 看是否是PKL XML
                TiXmlText *p_id = pkl_handle.
                    FirstChild(PKL_ELEM).
                    FirstChild(ID_ELEM).
                    FirstChild().
                    Text();
                if( p_id != 0)      // 是PKL
                {
                    p_assetmap->p_asset_[i].type_ = ASSET_PKL;
                    p_assetmap->p_asset_[i].is_complete_ = 1; // 认为都是完整的
                }
            }

            i++;
        } // for(; p_asset != 0; p_asset= p_asset->NextSiblingElement("Asset"))

        for(i=0;i<asset_num;i++)
        {
            if(p_assetmap->p_asset_[i].type_ == ASSET_PKL)
            {
                ParsePklInfo(i, p_assetmap);
            }
        }
    }
    catch( std::exception &e)
    {
        fprintf(stderr, "EXCEPT: {std} %s\n", e.what());
        return -1;
    }
    catch(...)
    {
        fprintf(stderr,  "EXCEPT: unknown\n");
        return -1;
    }
    //fprintf(stderr, "[%s:%d]asset_num:%d\n", ST_INFO, asset_num);
    return 0;
}

int ClearAssetMap( assetmap_info_t *p_assetmap)
{
    if(p_assetmap == NULL)return 0;

    if(p_assetmap->path_)
    {
        free(p_assetmap->path_);
        p_assetmap->path_=0;
    }
        
    for(int i=0; (unsigned int)i<p_assetmap->asset_num_ ;i++)
    {
        if(p_assetmap->p_asset_[i].path_)
        {
            free(p_assetmap->p_asset_[i].path_);
            p_assetmap->p_asset_[i].path_ = NULL;
        }
    }
    
    if(p_assetmap->p_asset_)
    {
        free(p_assetmap->p_asset_);
        p_assetmap->p_asset_ = 0;
    }
    memset(p_assetmap, 0, sizeof(assetmap_info_t));
    return 0;
}

void PrintErrInfo(const char *s, int err_ret)
{
    switch(-err_ret)
    {
        ERR_CHECK(ERR_CPL_NO_CPL_XML);
        ERR_CHECK(ERR_CPL_NO_XML_FOUND);
        ERR_CHECK(ERR_CPL_GLOB_ERR);
        ERR_CHECK(ERR_CPL_BAD_CPL_ID);
        ERR_CHECK(ERR_CPL_NILL_CPL_ID);
        ERR_CHECK(ERR_CPL_NO_CPL_ID);
        ERR_CHECK(ERR_CPL_LOAD_XML);
        ERR_CHECK(ERR_ASSET_LOAD_XML);
        ERR_CHECK(ERR_ASSET_NO_ASSET);
        ERR_CHECK(ERR_ASSET_NO_PKL_FN);
        ERR_CHECK(ERR_ASSET_NO_PKL_FS);
        ERR_CHECK(ERR_ASSET_NO_PKL_UUID);
        ERR_CHECK(ERR_ASSET_NO_CPL_FN);
        ERR_CHECK(ERR_ASSET_NO_CPL_FS);
        ERR_CHECK(ERR_ASSET_NO_CPL_UUID);
        ERR_CHECK(ERR_ASSET_NO_AUDIO_FN);
        ERR_CHECK(ERR_ASSET_NO_AUDIO_FS);
        ERR_CHECK(ERR_ASSET_NO_AUDIO_UUID);
        ERR_CHECK(ERR_ASSET_NO_VIDEO_FN);
        ERR_CHECK(ERR_ASSET_NO_VIDEO_FS);
        ERR_CHECK(ERR_ASSET_NO_VIDEO_UUID);
        ERR_CHECK(ERR_PKL_NO_ASSET);
        ERR_CHECK(ERR_PKL_LOAD_XML);
        ERR_CHECK(ERR_ASSET_NOT_PKL);
    default:
        fprintf(stderr, "%s:unknow error %d\n", s, err_ret);
        break;
    }

    return ;
    
}



