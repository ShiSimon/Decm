#ifndef INCLUDED_WAVE_H
#define INCLUDED_WAVE_H

struct WAVE_HEADER
{
    /* wave chunk */
    char id[4];                 /* RIFF */
    unsigned int size;          /* wave filelen-8 */
    char type[4];               /* WAVE */
    
    /* format chunk */
    char fmt[4];                /* 'fmt ' */
    unsigned int fmt_size;      /* 16 */
    unsigned short tag;         /* 0x0001 */
    unsigned short channels;    /* 1,2 */
    unsigned int sample_rate;
    unsigned int byte_per_sencond;
    unsigned short block_align;
    unsigned short bit_per_sample;

    /* data chunk */
    char data_id[4];            /* data */
    unsigned int pcm_size;      /* pcm size*/
};

struct WAVE_HEADER_EXTENSIBLE
{
    /* wave chunk */
    char id[4];                 /* RIFF */
    unsigned int size;          /* wave filelen-8 */
    char type[4];               /* WAVE */
    
    /* format chunk */
    char fmt[4];                /* 'fmt ' */
    unsigned int fmt_size;      /* 16,18 or 40 */
    unsigned short tag;         /* 0xfffe = 65534 */
    unsigned short channels;    /* 6 */
    unsigned int sample_rate;
    unsigned int byte_per_sencond;
    unsigned short block_align;
    unsigned short bit_per_sample;

    unsigned short cbsize; //0 or 22
    unsigned short valid_bits_per_sample;
    unsigned int channel_mask;
    unsigned char subformat[16];

    /* data chunk */
    char data_id[4];            /* data */
    unsigned int pcm_size;      /* pcm size*/
};

extern int fill_wav_header(struct WAVE_HEADER * pHeader, unsigned long long pcm_size, int audio_channels, int audio_bits_per_sample, int audio_sampling_rate);
extern int fill_wav_header_extensible(struct WAVE_HEADER_EXTENSIBLE * pHeader, unsigned long long pcm_size, int audio_channels, int audio_bits_per_sample, int audio_sampling_rate);

#endif
