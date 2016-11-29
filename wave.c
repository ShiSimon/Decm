#include <stdio.h>
#include <string.h>
#include "wave.h"

static const char KSDATAFORMAT_SUBTYPE_PCM[] = {
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71
};

int fill_wav_header(struct WAVE_HEADER * pHeader, unsigned long long pcm_size, int audio_channels, int audio_bits_per_sample, int audio_sampling_rate)
{
    /* wave chunk */
    pHeader->id[0] = 'R';                 /* RIFF */
    pHeader->id[1] = 'I';   
    pHeader->id[2] = 'F';   
    pHeader->id[3] = 'F';  
    
    pHeader->size = pcm_size + 44 -8;     /* wave filelen-8 */

    pHeader->type[0] = 'W';               /* WAVE */
    pHeader->type[1] = 'A'; 
    pHeader->type[2] = 'V'; 
    pHeader->type[3] = 'E'; 
    
    /* format chunk */
    pHeader->fmt[0] = 'f';                /* 'fmt ' */
    pHeader->fmt[1] = 'm'; 
    pHeader->fmt[2] = 't'; 
    pHeader->fmt[3] = ' '; 

    pHeader->fmt_size = 16;               /* 16 */
    pHeader->tag = 0x0001;                /* 0x0001 */
    pHeader->channels = audio_channels;
    pHeader->sample_rate = audio_sampling_rate;
    pHeader->byte_per_sencond = audio_bits_per_sample/8*audio_channels*audio_sampling_rate;
    pHeader->block_align = audio_bits_per_sample/8*audio_channels;
    pHeader->bit_per_sample = audio_bits_per_sample;

    /* data chunk */
    pHeader->data_id[0] = 'd';            /* data */
    pHeader->data_id[1] = 'a';  
    pHeader->data_id[2] = 't';  
    pHeader->data_id[3] = 'a';  
    pHeader->pcm_size = pcm_size;         /* pcm size*/
    return 0;
}

int fill_wav_header_extensible(struct WAVE_HEADER_EXTENSIBLE * pHeader, unsigned long long pcm_size, int audio_channels, int audio_bits_per_sample, int audio_sampling_rate)
{
    /* wave chunk */
    pHeader->id[0] = 'R';                 /* RIFF */
    pHeader->id[1] = 'I';   
    pHeader->id[2] = 'F';   
    pHeader->id[3] = 'F';  
    
    pHeader->size = pcm_size + 68 -8;     /* wave filelen-8 */

    pHeader->type[0] = 'W';               /* WAVE */
    pHeader->type[1] = 'A'; 
    pHeader->type[2] = 'V'; 
    pHeader->type[3] = 'E'; 
    
    /* format chunk */
    pHeader->fmt[0] = 'f';                /* 'fmt ' */
    pHeader->fmt[1] = 'm'; 
    pHeader->fmt[2] = 't'; 
    pHeader->fmt[3] = ' '; 

    pHeader->fmt_size = 40;               /* 16 */
    pHeader->tag = 0xfffe;                /* 0x0001 */
    pHeader->channels = audio_channels;
    pHeader->sample_rate = audio_sampling_rate;
    pHeader->byte_per_sencond = audio_bits_per_sample/8*audio_channels*audio_sampling_rate;
    pHeader->block_align = audio_bits_per_sample/8*audio_channels;
    pHeader->bit_per_sample = audio_bits_per_sample;

    pHeader->cbsize = 22;
    pHeader->valid_bits_per_sample = audio_bits_per_sample;
    pHeader->channel_mask = 0x060F;
    memcpy(pHeader->subformat, KSDATAFORMAT_SUBTYPE_PCM, 16);

    /* data chunk */
    pHeader->data_id[0] = 'd';            /* data */
    pHeader->data_id[1] = 'a';  
    pHeader->data_id[2] = 't';  
    pHeader->data_id[3] = 'a';  
    pHeader->pcm_size = pcm_size;         /* pcm size*/
    return 0;   
}
