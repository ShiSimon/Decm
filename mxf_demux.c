#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "mylseek64.h"
#include "mxf_demux.h"

//#define _DEBUG

/**
 * Rational number num/den.
 */
typedef struct AVRational{
    int num; ///< numerator
    int den; ///< denominator
} AVRational;

enum MXFMetadataSetType {
    AnyType,
    MaterialPackage,
    SourcePackage,
    SourceClip,
    TimecodeComponent,
    Sequence,
    MultipleDescriptor,
    Descriptor,
    Track,
    CryptoContext,
    Preface,
    Identification,
    ContentStorage,
    SubDescriptor,
    IndexTableSegment,
    EssenceContainerData,
    TypeBottom,// add metadata type before this
};

typedef struct {
    UID uid;
    enum MXFMetadataSetType type;
} MXFMetadataSet;

typedef struct {
    UID uid;
    enum MXFMetadataSetType type;
    UID essence_container_ul;
    UID essence_codec_ul;
    AVRational sample_rate;
    AVRational aspect_ratio;
    int width;
    int height;
    int channels;
    int bits_per_sample;
    UID *sub_descriptors_refs;
    int sub_descriptors_count;
    int linked_track_id;
    uint8_t *extradata;
    int extradata_size;
} MXFDescriptor;

typedef struct {
    UID uid;
    enum MXFMetadataSetType type;
    uint64_t start_position;
    uint64_t duration;
    uint32_t editunit_bytecount;
    uint32_t delta_entry_number;
    uint32_t delta_entry_unitsize;
    uint32_t index_entry_number;
    uint32_t index_entry_unitsize;
} MXFIndexTableSegment;

typedef struct {
    const UID key;
    int (*read)();
    int ctx_size;
    enum MXFMetadataSetType type;
} MXFMetadataReadTableEntry;

typedef struct {
    UID uid;
    uint16_t major_version;
    uint16_t minor_version;
    uint32_t kag_size;
    uint64_t this_partition;
    uint64_t previous_partition;
    uint64_t footer_partition;
    uint64_t header_bytecount;
    uint64_t index_bytecount;
    uint32_t index_sid;
    uint64_t body_offset;
    uint32_t body_sid;
    UID      operational_pattern;
    uint32_t  container_element_count;
    uint32_t  container_element_size;
} MXFPartitionPack;

/* partial keys to match */
static const uint8_t mxf_header_partition_pack_key[]       = { 0x06,0x0e,0x2b,0x34,0x02,0x05,0x01,0x01,0x0d,0x01,0x02,0x01,0x01,0x02 };
static const uint8_t mxf_footer_partition_pack_key[]       = { 0x06,0x0e,0x2b,0x34,0x02,0x05,0x01,0x01,0x0d,0x01,0x02,0x01,0x01,0x04 };
static const uint8_t mxf_essence_element_key[]             = { 0x06,0x0e,0x2b,0x34,0x01,0x02,0x01,0x01,0x0d,0x01,0x03,0x01 };
static const uint8_t mxf_klv_key[]                         = { 0x06,0x0e,0x2b,0x34 };
/* complete keys to match */
static const uint8_t mxf_crypto_source_container_ul[]      = { 0x06,0x0e,0x2b,0x34,0x01,0x01,0x01,0x09,0x06,0x01,0x01,0x02,0x02,0x00,0x00,0x00 };
static const uint8_t mxf_encrypted_triplet_key[]           = { 0x06,0x0e,0x2b,0x34,0x02,0x04,0x01,0x07,0x0d,0x01,0x03,0x01,0x02,0x7e,0x01,0x00 };
static const uint8_t mxf_encrypted_essence_container[]     = { 0x06,0x0e,0x2b,0x34,0x04,0x01,0x01,0x07,0x0d,0x01,0x03,0x01,0x02,0x0b,0x01,0x00 };
static const uint8_t mxf_sony_mpeg4_extradata[]            = { 0x06,0x0e,0x2b,0x34,0x04,0x01,0x01,0x01,0x0e,0x06,0x06,0x02,0x02,0x01,0x00,0x00 };

static const uint8_t mxf_mpeg2_descriptor[]                = { 0x06,0x0E,0x2B,0x34,0x02,0x53,0x01,0x01,0x0d,0x01,0x01,0x01,0x01,0x01,0x51,0x00 };
static const uint8_t mxf_jpeg2000_descriptor[]             = { 0x06,0x0E,0x2B,0x34,0x02,0x53,0x01,0x01,0x0d,0x01,0x01,0x01,0x01,0x01,0x5a,0x00 };
static const uint8_t mxf_h264_descriptor[]                 = { 0x06,0x0E,0x2B,0x34,0x02,0x53,0x01,0x01,0x0d,0x01,0x01,0x01,0x01,0x01,0x61,0x00 }; //Only For WuXi Taihang

static const uint8_t mxf_wav_descriptor[]                  = { 0x06,0x0E,0x2B,0x34,0x02,0x53,0x01,0x01,0x0d,0x01,0x01,0x01,0x01,0x01,0x48,0x00 };
static const uint8_t mxf_ac3_descriptor[]                  = { 0x06,0x0E,0x2B,0x34,0x02,0x53,0x01,0x01,0x0d,0x01,0x01,0x01,0x01,0x01,0x49,0x00 }; //Only For SMET DCP tools

static int mxf_read_metadata_generic_descriptor(MXFDescriptor *descriptor, int fd, KLVPacket *klv, int tag, int size);
static int mxf_read_index_table_segment(MXFIndexTableSegment *segment, int fd, KLVPacket *klv, int tag, int size);

static const MXFMetadataReadTableEntry mxf_metadata_read_table[] = {
    { { 0x06,0x0E,0x2B,0x34,0x02,0x53,0x01,0x01,0x0d,0x01,0x01,0x01,0x01,0x01,0x51,0x00 }, mxf_read_metadata_generic_descriptor, sizeof(MXFDescriptor), Descriptor }, /* MPEG 2 Video */
    { { 0x06,0x0E,0x2B,0x34,0x02,0x53,0x01,0x01,0x0d,0x01,0x01,0x01,0x01,0x01,0x5a,0x00 }, mxf_read_metadata_generic_descriptor, sizeof(MXFDescriptor), Descriptor }, /* JPEG 2000 Video */
    { { 0x06,0x0E,0x2B,0x34,0x02,0x53,0x01,0x01,0x0d,0x01,0x01,0x01,0x01,0x01,0x48,0x00 }, mxf_read_metadata_generic_descriptor, sizeof(MXFDescriptor), Descriptor }, /* Wave */
    { { 0x06,0x0E,0x2B,0x34,0x02,0x53,0x01,0x01,0x0d,0x01,0x02,0x01,0x01,0x10,0x01,0x00 }, mxf_read_index_table_segment, sizeof(MXFIndexTableSegment), IndexTableSegment },

    { { 0x06,0x0E,0x2B,0x34,0x02,0x53,0x01,0x01,0x0d,0x01,0x01,0x01,0x01,0x01,0x61,0x00 }, mxf_read_metadata_generic_descriptor, sizeof(MXFDescriptor), Descriptor }, /* H.264 Video, defined by WuXi Taihang */
    { { 0x06,0x0E,0x2B,0x34,0x02,0x53,0x01,0x01,0x0d,0x01,0x01,0x01,0x01,0x01,0x49,0x00 }, mxf_read_metadata_generic_descriptor, sizeof(MXFDescriptor), Descriptor }, /* AC3, defined by SMET DCP tools */
};

const MXFCodecUL ff_mxf_codec_uls[] = {
    /* PictureEssenceCoding */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x03,0x04,0x01,0x02,0x02,0x01,0x01,0x11,0x00 }, 14, CODEC_ID_MPEG2VIDEO }, /* MP@ML Long GoP */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x01,0x04,0x01,0x02,0x02,0x01,0x02,0x01,0x01 }, 14, CODEC_ID_MPEG2VIDEO }, /* D-10 50Mbps PAL */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x03,0x04,0x01,0x02,0x02,0x01,0x03,0x03,0x00 }, 14, CODEC_ID_MPEG2VIDEO }, /* MP@HL Long GoP */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x03,0x04,0x01,0x02,0x02,0x01,0x04,0x02,0x00 }, 14, CODEC_ID_MPEG2VIDEO }, /* 422P@HL I-Frame */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x03,0x04,0x01,0x02,0x02,0x01,0x20,0x02,0x03 }, 14,      CODEC_ID_MPEG4 }, /* XDCAM proxy_pal030926.mxf */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x01,0x04,0x01,0x02,0x02,0x02,0x01,0x02,0x00 }, 13,    CODEC_ID_DVVIDEO }, /* DV25 IEC PAL */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x07,0x04,0x01,0x02,0x02,0x03,0x01,0x01,0x00 }, 14,   CODEC_ID_JPEG2000 }, /* JPEG2000 Codestream */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x01,0x04,0x01,0x02,0x01,0x7F,0x00,0x00,0x00 }, 13,   CODEC_ID_RAWVIDEO }, /* Uncompressed */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x01,0x04,0x01,0x02,0x02,0x03,0x02,0x00,0x00 }, 14,      CODEC_ID_DNXHD }, /* SMPTE VC-3/DNxHD */
    /* SoundEssenceCompression */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x01,0x04,0x02,0x02,0x01,0x00,0x00,0x00,0x00 }, 13,  CODEC_ID_PCM_S16LE }, /* Uncompressed */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x01,0x04,0x02,0x02,0x01,0x7F,0x00,0x00,0x00 }, 13,  CODEC_ID_PCM_S16LE },
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x07,0x04,0x02,0x02,0x01,0x7E,0x00,0x00,0x00 }, 13,  CODEC_ID_PCM_S16BE }, /* From Omneon MXF file */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x04,0x04,0x02,0x02,0x02,0x03,0x01,0x01,0x00 }, 15,   CODEC_ID_PCM_ALAW }, /* XDCAM Proxy C0023S01.mxf */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x01,0x04,0x02,0x02,0x02,0x03,0x02,0x01,0x00 }, 15,        CODEC_ID_AC3 },
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x01,0x04,0x02,0x02,0x02,0x03,0x02,0x05,0x00 }, 15,        CODEC_ID_MP2 }, /* MP2 or MP3 */
  //{ { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x01,0x04,0x02,0x02,0x02,0x03,0x02,0x1C,0x00 }, 15,    CODEC_ID_DOLBY_E }, /* Dolby-E */
    { { 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 },  0,       CODEC_ID_NONE },
};

#define IS_KLV_KEY(x, y) (!memcmp(x, y, sizeof(y)))
#define PRINT_KEY(s, x) fprintf(stderr, "%s %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n", s, \
                             (x)[0], (x)[1], (x)[2], (x)[3], (x)[4], (x)[5], (x)[6], (x)[7], (x)[8], (x)[9], (x)[10], (x)[11], (x)[12], (x)[13], (x)[14], (x)[15])

/* 判断是否是文件末尾，是末尾则返回0，其他返回非0 */
static int is_eof(int fd)
{
    int ret;
    char buf[1];
    
    ret = read(fd, buf, 1);
    if(ret == 0)ret = 0;
    else
    {
        lseek_64(fd, -1, SEEK_CUR);
        ret = 1;
    }
    return ret;
}

/* 返回文件的读写位置 */
static int64_t my_tell(int fd)
{
    return (int64_t)lseek_64(fd, 0, SEEK_CUR);
}

static uint64_t get_byte(int fd)
{
    unsigned char buf[1];
    uint64_t one_byte;
    int tmp_ret = 0;
    tmp_ret = read(fd, buf, 1);
    if(tmp_ret != 1)
    {
    }
    one_byte = (uint64_t)(buf[0]);
    return one_byte;
}

static uint64_t get_be16(int fd)
{
    uint64_t val;
    val = get_byte(fd)<<8;
    val |= get_byte(fd);
    return val;
}

static uint64_t get_be32(int fd)
{
    uint64_t val;
    val = get_be16(fd)<<16;
    val |= get_be16(fd);
    return val;
}

static uint64_t get_be64(int fd)
{
    uint64_t val;
    val = get_be32(fd)<<32;
    val |= get_be32(fd);
    return val;
}

static void get_buffer(int fd, unsigned char *buf, int size)
{
    int tmp_ret = 0;
    unsigned char tmp_buf[size];
    tmp_ret = read(fd, tmp_buf, size);
    if(tmp_ret != size)
    {
    }
    memcpy(buf, tmp_buf, size);
    return;
    
}

static void mxf_read_metadata_pixel_layout(int fd, MXFDescriptor *descriptor)
{
    int code;

    do {
        code = get_byte(fd);
        fprintf(stderr, "pixel layout: code 0x%x\n", code);
        switch (code) {
        case 0x52: /* R */
            descriptor->bits_per_sample += get_byte(fd);
            break;
        case 0x47: /* G */
            descriptor->bits_per_sample += get_byte(fd);
            break;
        case 0x42: /* B */
            descriptor->bits_per_sample += get_byte(fd);
            break;
        default:
            get_byte(fd);
        }
    } while (code != 0); /* SMPTE 377M E.2.46 */
}

/*
 * Match an uid independently of the version byte and up to len common bytes
 * Returns: boolean
 */
static int mxf_match_uid(const UID key, const UID uid, int len)
{
    int i;
    for (i = 0; i < len; i++) {
        if (i != 7 && key[i] != uid[i])
            return 0;
    }
    return 1;
}

/* return code ID */
static int mxf_get_codec_ul(const MXFCodecUL *uls, UID *uid)
{
    while (uls->uid[0]) {
        if(mxf_match_uid(uls->uid, *uid, uls->matching_len))
            break;
        uls++;
    }
    return uls->id;
}

static int mxf_read_partition_pack(int fd, MXFPartitionPack *header)
{
    int ret = 0;
    UID ul1;
    UID ul2;

    header->major_version = get_be16(fd);
    header->minor_version = get_be16(fd);
    header->kag_size = get_be32(fd);
    header->this_partition = get_be64(fd);
    header->previous_partition = get_be64(fd);
    header->footer_partition = get_be64(fd);
    header->header_bytecount = get_be64(fd);
    header->index_bytecount = get_be64(fd);
    header->index_sid = get_be32(fd);
    header->body_offset = get_be64(fd);
    header->body_sid = get_be32(fd);
    get_buffer(fd, header->operational_pattern, 16);
    header->container_element_count = get_be32(fd);
    header->container_element_size = get_be32(fd);
    get_buffer(fd, ul1, 16);
    get_buffer(fd, ul2, 16);

    /* fprintf(stderr, "major=%u,minor=%u, foot=%llu, bodysid=%d, contain count=%u,size=%u\n", */
    /*         header->major_version,  */
    /*         header->minor_version, */
    /*         header->footer_partition, */
    /*         header->body_sid, */
    /*         header->container_element_count, */
    /*         header->container_element_size); */
    return ret;
}

static int mxf_read_metadata_generic_descriptor(MXFDescriptor *descriptor, int fd, KLVPacket *klv, int tag, int size)
{
    int codecID = CODEC_ID_NONE;
    switch(tag)
    {
    case 0x3F01:
        descriptor->sub_descriptors_count = get_be32(fd);
        //#define UINT_MAX 4294967295U
        if ((uint32_t)descriptor->sub_descriptors_count >= 4294967295U / sizeof(UID))
            return -1;
        descriptor->sub_descriptors_refs = malloc(descriptor->sub_descriptors_count * sizeof(UID));
        lseek_64(fd, 4, SEEK_CUR); /* useless size of objects, always 16 according to specs */
        get_buffer(fd, (uint8_t *)descriptor->sub_descriptors_refs, descriptor->sub_descriptors_count * sizeof(UID));
        free(descriptor->sub_descriptors_refs);descriptor->sub_descriptors_refs = NULL;
        break;
    case 0x3004:
        get_buffer(fd, descriptor->essence_container_ul, 16);
        break;
    case 0x3006:
        descriptor->linked_track_id = get_be32(fd);
        break;
    case 0x3201: /* PictureEssenceCoding */
        fprintf(stderr, "%s\n", __FUNCTION__);
        get_buffer(fd, descriptor->essence_codec_ul, 16);
        PRINT_KEY("picture codec", descriptor->essence_codec_ul);
        codecID = mxf_get_codec_ul(ff_mxf_codec_uls, &descriptor->essence_codec_ul);
        if(codecID == CODEC_ID_NONE) {}
        break;
    case 0x3203:
        descriptor->width = get_be32(fd);
        break;
    case 0x3202:
        descriptor->height = get_be32(fd);
        break;
    case 0x320E:
        descriptor->aspect_ratio.num = get_be32(fd);
        descriptor->aspect_ratio.den = get_be32(fd);
        break;
    case 0x3D03:
        descriptor->sample_rate.num = get_be32(fd);
        descriptor->sample_rate.den = get_be32(fd);
        klv->sampling_rate = descriptor->sample_rate.num/descriptor->sample_rate.den;
        break;
    case 0x3D06: /* SoundEssenceCompression */
        get_buffer(fd, descriptor->essence_codec_ul, 16);
        //PRINT_KEY("sound codec", descriptor->essence_codec_ul);
        codecID = mxf_get_codec_ul(ff_mxf_codec_uls, &descriptor->essence_codec_ul);
        if(codecID == CODEC_ID_NONE) {}
        break;
    case 0x3D07:
        descriptor->channels = get_be32(fd);
        klv->channels = descriptor->channels;
        break;
    case 0x3D01:
        descriptor->bits_per_sample = get_be32(fd);
        klv->bits_per_sample = descriptor->bits_per_sample;
        break;
    case 0x3401:
        mxf_read_metadata_pixel_layout(fd, descriptor);
        break;
    case 0x8201: /* Private tag used by SONY C0023S01.mxf */
        descriptor->extradata = malloc(size);
        descriptor->extradata_size = size;
        get_buffer(fd, descriptor->extradata, size);
        free(descriptor->extradata);descriptor->extradata = NULL;
        break;
    }
    return 0;
}

static int mxf_read_index_table_segment(MXFIndexTableSegment *segment, int fd, KLVPacket *klv, int tag, int size)
{
    uint8_t one_byte = 0;
    uint64_t pos = 0;
    
    switch(tag) 
    {
    case 0x3F0B: 
#ifdef _DEBUG
        fprintf(stderr, "IndexEditRate %llu/%llu\n", (long long unsigned int)get_be32(fd), (long long unsigned int)get_be32(fd));
#endif
        break;
    case 0x3F0C: 
        segment->start_position = get_be64(fd);
#ifdef _DEBUG
        fprintf(stderr, "IndexStartPosition %llu\n", (long long unsigned int)segment->start_position); 
#endif
        break;
    case 0x3F0D: 
        segment->duration =  get_be64(fd);
#ifdef _DEBUG
        fprintf(stderr, "IndexDuration %llu\n", (long long unsigned int)segment->duration); 
#endif
        break;
    case 0x3F05: 
        segment->editunit_bytecount = get_be32(fd);
#ifdef _DEBUG
        fprintf(stderr, "EditUnitByteCount %u\n", segment->editunit_bytecount); 
#endif
        if(segment->editunit_bytecount > 0)
        {
            klv->pos = (uint64_t)segment->editunit_bytecount*(uint64_t)klv->index;
            klv->found = 1;
        }
#ifdef _DEBUG
        fprintf(stderr, "found indexSegment pos=%llu\n", (long long unsigned int)klv->pos);
#endif
        break;
    case 0x3F06: 
#ifdef _DEBUG
        fprintf(stderr, "IndexSID %llu\n", (long long unsigned int)get_be32(fd));
#endif
        break;
    case 0x3F07:
#ifdef _DEBUG
        fprintf(stderr, "BodySID %llu\n", (long long unsigned int)get_be32(fd)); 
#endif
        break;
    case 0x3F08: 
#ifdef _DEBUG
        fprintf(stderr, "SliceCount %llu\n", (long long unsigned int)get_byte(fd)); 
#endif
        break;
    case 0x3F0E: 
#ifdef _DEBUG
        fprintf(stderr, "PosTableCount %llu\n", (long long unsigned int)get_byte(fd)); 
#endif
        break;
    case 0x3F09: 
        segment->delta_entry_number = get_be32(fd);
        segment->delta_entry_unitsize = get_be32(fd);
#ifdef _DEBUG
        fprintf(stderr, "DeltaEntryArray count=%u unitsize=%u\n", segment->delta_entry_number, segment->delta_entry_unitsize); 
#endif
        break;
    case 0x3F0A: 
        segment->index_entry_number = get_be32(fd);
        segment->index_entry_unitsize = get_be32(fd);
#ifdef _DEBUG
        fprintf(stderr, "IndexEntryArray count=%u unitsize=%u\n\n", segment->index_entry_number, segment->index_entry_unitsize); 
#endif
        if( klv->index >= segment->start_position  &&  klv->index < (segment->start_position+segment->duration) )
        {
            uint64_t offset = (klv->index - segment->start_position)*segment->index_entry_unitsize;
            lseek_64(fd, offset, SEEK_CUR);
            one_byte = get_byte(fd);
            one_byte = get_byte(fd);
            one_byte = get_byte(fd);
            if(one_byte)
            {
                //skip warning
            }
            pos = get_be64(fd);
            klv->pos = pos;
            klv->found = 1;
#ifdef _DEBUG
            fprintf(stderr, "found indexSegment pos=%llu\n", (long long unsigned int)klv->pos);
#endif
        }
        break;
    }
    return 0;
}

//读取KLV结构的length
//return: int64_t:length  -1:bytes_num超过8字节  
static uint64_t klv_decode_ber_length(int fd)
{
    uint64_t size = get_byte(fd);
    if (size & 0x80) { /* long form */
        int bytes_num = size & 0x7f;
        /* SMPTE 379M 5.3.4 guarantee that bytes_num must not exceed 8 bytes */
        if (bytes_num > 8)
            return -1;
        size = 0;
        while (bytes_num--)
            size = size << 8 | get_byte(fd);
    }
    return size;
}

//寻找4字节头,指针移到size后
//return:  1:找到了对应的key  0:没找到 
static int mxf_read_sync(int fd, const uint8_t *key, int size)
{
    int i, b;
    for (i = 0; i < size && is_eof(fd); i++)
    {
        b = get_byte(fd);
        if (b == key[0])
            i = 0;
        else if (b != key[i])
            i = -1;
    }
    return i == size;
}

//获取KLV包的信息
//retrun:  -1:bytes_num超过8字节 0:成功 -2:没找到同步头
static int klv_read_packet(KLVPacket *klv, int fd)
{
    if (!mxf_read_sync(fd, mxf_klv_key, 4))
        return -2;
    klv->offset = my_tell(fd) - 4;
    memcpy(klv->key, mxf_klv_key, 4);
    get_buffer(fd, klv->key + 4, 12);
    klv->length = klv_decode_ber_length(fd);
    klv->end = my_tell(fd) + klv->length;
    return (int64_t)(klv->length) == -1 ? -1 : 0;
}

//return:  -1:错误  0:成功
static int mxf_decrypt_triplet(int fd, KLVPacket *klv)
{
    uint64_t size;
    uint64_t plaintext_size;
    //int index;
    
    // crypto context
    size = klv_decode_ber_length(fd);
    lseek_64(fd, size, SEEK_CUR);
    // plaintext offset
    size = klv_decode_ber_length(fd);
    plaintext_size = get_be64(fd);
    // source klv key
    size = klv_decode_ber_length(fd);

    get_buffer(fd, klv->key, 16);

#ifdef _DEBUG
    //PRINT_KEY("triplet essence key", klv->key);
#endif
    if (!IS_KLV_KEY(klv->key, mxf_essence_element_key))
    {
        printf("mxf_decrypt_triplet: not a mxf_essence_element_key\n");
        fflush(stdout);
        return -1;
    }
    
    //index = mxf_get_stream_index(s, klv);
   
    // source size
    klv_decode_ber_length(fd);
    klv->orig_size = get_be64(fd);
    if (klv->orig_size < plaintext_size)
    {
        printf("mxf_decrypt_triplet: orig_size < plaintext_size\n");
        fflush(stdout);    
        return -1;
    }
    
    // enc. code
    size = klv_decode_ber_length(fd);
    if (size < 32 || size - 32 < klv->orig_size)
    {
        printf("mxf_decrypt_triplet: size < 32 || size - 32 < orig_size\n");
        fflush(stdout);  
        return -1;
    }
    
    get_buffer(fd, klv->ivec, 16);
    get_buffer(fd, klv->checkbuf, 16);

    klv->plain_offset = my_tell(fd);
    klv->plain_size = plaintext_size;
    
    size -= 32;
    size -= plaintext_size;
    
    klv->aes_offset = klv->plain_offset+plaintext_size;
    klv->aes_size = size;
    return 0;
}


//return:  -2:文件结束  0:成功读取一个ES包 -1:bytes_num超过8字节 -3:triplet出错
int mxf_read_packet(int fd, KLVPacket *klv)
{
    int ret;
        
    while (is_eof(fd)) 
    {
        if ((ret = klv_read_packet(klv, fd)) < 0)
            return -1;

#ifdef _DEBUG
        PRINT_KEY("read packet", klv->key);
#endif

        if (IS_KLV_KEY(klv->key, mxf_encrypted_triplet_key))
        {
#ifdef _DEBUG
            fprintf(stderr,"mxf_read_packet:now encrypted_triplet_key\n");
#endif
            klv->type = TRIPLET_PACKET;
            klv->end = my_tell(fd) + klv->length;
            int res = mxf_decrypt_triplet(fd, klv);
            if(res < 0)
            {
                fprintf(stderr,"mxf_read_packet:triplet error\n");
                return -3;
            }
            return 0;
        }
        if (IS_KLV_KEY(klv->key, mxf_essence_element_key))
        {
#ifdef _DEBUG
            fprintf(stderr,"mxf_read_packet:now essence_element_key\n");
#endif
            klv->type = ESSENCE_PACKET;
            klv->plain_size = 0;
            klv->aes_size = 0;
            klv->end = my_tell(fd) + klv->length;
            //int index = mxf_get_stream_index(s, &klv);
            //fprintf(stderr,"now mxf_read_packet essence_key*****\n");
            //fprintf(stderr,"now klv offset:%llu length:%llu*****\n", klv->offset, klv->length);
           
            /* check for 8 channels AES3 element */
            if (klv->key[12] == 0x06 && klv->key[13] == 0x01 && klv->key[14] == 0x10)
            {
               
                fprintf(stderr,"mxf_read_packet:now AES_key\n");
                goto skip;
            } 
            else
            {
                return 0;                
            }
        }
        if (IS_KLV_KEY(klv->key, mxf_footer_partition_pack_key))
        {
            fprintf(stderr, "%s:end of the file\n", __FUNCTION__);
            return -2;
        }
        
//是别的key
        fprintf(stderr,"mxf_read_packet: unknow packet\n");
        skip:
        lseek_64(fd, klv->length, SEEK_CUR);
    }
    return -2;
}

static int mxf_read_local_tags(int fd, KLVPacket *klv, int (*read_child)(), int ctx_size, enum MXFMetadataSetType type)
{
    MXFMetadataSet *ctx = malloc(ctx_size);
    uint64_t klv_end= my_tell(fd) + klv->length;
    
    memset(ctx, 0, ctx_size);

    while ((uint64_t)my_tell(fd) + 4 < klv_end)
    {
        int tag = get_be16(fd);
        int size = get_be16(fd); /* KLV specified by 0x53 */
        uint64_t next= my_tell(fd) + size;

        if (!size) 
        { /* ignore empty tag, needed for some files with empty UMID tag */
            fprintf(stderr, "local tag 0x%04X with 0 size\n", tag);
            continue;
        }
        if(ctx_size && tag == 0x3C0A)
            get_buffer(fd, ctx->uid, 16);
        else
            read_child(ctx, fd, klv, tag, size);

        lseek_64(fd, next, SEEK_SET);
    }
    if (ctx_size) ctx->type = type;
    free(ctx);
    return 0;
}

//retrurn:  -1:bytes_num超过8字节  0:成功
int mxf_read_header(int fd, KLVPacket *klv)
{
    if (!mxf_read_sync(fd, mxf_header_partition_pack_key, 14))
    {
        fprintf(stderr,"could not find header partition pack key\n");
        return -1;
    }
    lseek_64(fd, -14, SEEK_CUR);

    klv->codecID = CODEC_ID_NONE;
    
    while (is_eof(fd))
    {
        const MXFMetadataReadTableEntry *metadata;
        
        if (klv_read_packet(klv, fd) < 0)
            return -1;

#ifdef _DEBUG
        //PRINT_KEY("read header", klv->key);
#endif
        if (IS_KLV_KEY(klv->key, mxf_encrypted_triplet_key) ||
            IS_KLV_KEY(klv->key, mxf_essence_element_key))
        {
            /* FIXME avoid seek */
            lseek_64(fd, klv->offset, SEEK_SET);
            break;
        }

        if(mxf_match_uid(mxf_mpeg2_descriptor, klv->key, 15))
        {
            klv->codecID = CODEC_ID_MPEG2VIDEO;
            memcpy(klv->descriptor, klv->key, 16);
        }
        else if(mxf_match_uid(mxf_jpeg2000_descriptor, klv->key, 15))
        {
            klv->codecID = CODEC_ID_JPEG2000;
            memcpy(klv->descriptor, klv->key, 16);
        }
        else if(mxf_match_uid(mxf_ac3_descriptor, klv->key, 15))
        {
            klv->codecID = CODEC_ID_AC3;
            memcpy(klv->descriptor, klv->key, 16);
        }
        else if(mxf_match_uid(mxf_wav_descriptor, klv->key, 15))
        {
            klv->codecID = CODEC_ID_PCM;
            memcpy(klv->descriptor, klv->key, 16);
        }
        else if(mxf_match_uid(mxf_h264_descriptor, klv->key, 15))
        {
            klv->codecID = CODEC_ID_H264;
            memcpy(klv->descriptor, klv->key, 16);
        }
        
        for (metadata = mxf_metadata_read_table; metadata->read; metadata++)
        {
            if (IS_KLV_KEY(klv->key, metadata->key))
            {
                if (mxf_read_local_tags(fd, klv, metadata->read, metadata->ctx_size, metadata->type) < 0)
                {
                    fprintf(stderr, "error reading header metadata\n");
                    return -1;
                }
                break;
            }
        }

        if (!metadata->read)
            lseek_64(fd, klv->length, SEEK_CUR);
    }
    
    return 0;
}

//跳过一个packet
//return:  -2:文件结束  0:成功跳过 -1:bytes_num超过8字节
int mxf_skip_packet(int fd, KLVPacket *klv)
{
    int ret = 0;
        
    while (is_eof(fd)) 
    {
        if ((ret = klv_read_packet(klv, fd)) < 0)
        {
            return -1;
        }
        
        lseek_64(fd, klv->length, SEEK_CUR);
        return 0;
    }
    return -2;
}

/* ret <0:error  =0:ok */
int mxf_get_index_position(const char *filename, uint32_t index, uint64_t *pos)
{
    int ret = 0;
    KLVPacket klv_packet;
    KLVPacket *klv = &klv_packet;
    MXFPartitionPack header;
    MXFPartitionPack footer;
    int footer_parsed = 0;
    uint64_t body_offset = 0;
    int fd = 0;

    memset(&header, 0, sizeof(header));
    memset(&footer, 0, sizeof(footer));
    memset(klv, 0, sizeof(KLVPacket));

    fd = open(filename, O_RDONLY | O_LARGEFILE);
    if(fd == -1)
    {
        perror("open");
        return -1;
    }

    klv->found = 0;
    klv->index = index;
        
    if (!mxf_read_sync(fd, mxf_header_partition_pack_key, 14))
    {
        fprintf(stderr,"could not find header partition pack key\n");
        return -1;
    }
    lseek_64(fd, -14, SEEK_CUR);

    /* get header partition */
    if (klv_read_packet(klv, fd) < 0)
        return -1;

    memcpy(header.uid, klv->key, 16);
    mxf_read_partition_pack(fd, &header);
    lseek_64(fd, klv->end, SEEK_SET);

    body_offset = header.header_bytecount + klv->end;

read_metadata__:
    while (is_eof(fd))
    {
        const MXFMetadataReadTableEntry *metadata;
        
        if (klv_read_packet(klv, fd) < 0)
            return -1;

        if (IS_KLV_KEY(klv->key, mxf_encrypted_triplet_key) ||
            IS_KLV_KEY(klv->key, mxf_essence_element_key))
        {
            /* FIXME avoid seek */
            lseek_64(fd, klv->offset, SEEK_SET);
            break;
        }

        for (metadata = mxf_metadata_read_table; metadata->read; metadata++)
        {
            if (IS_KLV_KEY(klv->key, metadata->key))
            {
                if (mxf_read_local_tags(fd, klv, metadata->read, metadata->ctx_size, metadata->type) < 0)
                {
                    fprintf(stderr, "error reading header metadata\n");
                    return -1;
                }
                break;
            }
        }

        if (!metadata->read)
            lseek_64(fd, klv->length, SEEK_CUR);
    }

    if(klv->found == 0 && footer_parsed == 0)
    {
        lseek_64(fd, header.footer_partition, SEEK_SET); 
        /* get footer partition */
        if (klv_read_packet(klv, fd) < 0)
            return -1;
        
        memcpy(footer.uid, klv->key, 16);
        mxf_read_partition_pack(fd, &footer);
        lseek_64(fd, klv->end, SEEK_SET);
        footer_parsed = 1;
        goto read_metadata__;
    }

    if(klv->found)
    {
        klv->pos += body_offset;
        //fprintf(stderr, "mxf_get_index_position: found pos=%llu\n", (unsigned long long)klv->pos);
        *pos = klv->pos;
    }
    else
    {
        //如果没找到，直接赋予footer起始位置，通知上层找不到音视频数据，上层以此来判断
        *pos = header.footer_partition;
    }
    
    if(fd > 0)
    {
        close(fd);
    }
    return ret;
}

/* ret <0:error  =0:ok */
int mxf_get_position(const char *filename, uint64_t *pbody, uint64_t *pfooter)
{
    int ret = 0;
    KLVPacket klv_packet;
    KLVPacket *klv = &klv_packet;
    MXFPartitionPack header;
    uint64_t body_offset = 0;
    int fd = 0;

    memset(&header, 0, sizeof(header));
    memset(klv, 0, sizeof(KLVPacket));

    fd = open(filename, O_RDONLY | O_LARGEFILE);
    if(fd == -1)
    {
        perror("open");
        return -1;
    }
        
    if (!mxf_read_sync(fd, mxf_header_partition_pack_key, 14))
    {
        fprintf(stderr,"could not find header partition pack key\n");
        close(fd);
        return -1;
    }
    lseek_64(fd, -14, SEEK_CUR);

    /* get header partition */
    if (klv_read_packet(klv, fd) < 0)
    {
        close(fd);
        return -1;
    }

    memcpy(header.uid, klv->key, 16);
    mxf_read_partition_pack(fd, &header);
    lseek_64(fd, klv->end, SEEK_SET);

    body_offset = header.header_bytecount + klv->end;
    *pbody = body_offset;
    *pfooter = header.footer_partition;

    if(fd > 0)
    {
        close(fd);
    }
    return ret;
}
