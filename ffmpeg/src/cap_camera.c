/*
 * Copyright (c) 2010 Nicolas George
 * Copyright (c) 2011 Stefano Sabatini
 * Copyright (c) 2014 Andrey Utkin
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/**
 * @file
 * API example for demuxing, decoding, filtering, encoding and muxing
 * @example transcoding.c
 */
 
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include "libswscale/swscale.h"

static AVFormatContext *ifmt_ctx=NULL;
static AVFormatContext *ofmt_ctx=NULL;

static AVCodecContext *dec_ctx=NULL;
static AVCodecContext *enc_ctx=NULL;


//-----------------------------------------------------
#include <SDL2/SDL.h>
SDL_Window* sdlscreen=NULL;
SDL_Renderer* sdlRenderer=NULL;
SDL_Texture* sdlTexture=NULL;


int sdl_windows_init()
{
    SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_TIMER);
    sdlscreen = SDL_CreateWindow("windows", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 640, 480, SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);
    sdlRenderer=SDL_CreateRenderer(sdlscreen, -1, 0);
	//SDL_PIXELFORMAT_YUY2  SDL_PIXELFORMAT_YV12  SDL_PIXELFORMAT_BGR888 SDL_PIXELFORMAT_YV12 SDL_PIXELFORMAT_IYUV
    sdlTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING,640,480);
	
    return 0;
}

int sdl_show(AVFrame *show_frame)
{
	SDL_UpdateYUVTexture(sdlTexture,NULL,show_frame->data[0],640,show_frame->data[1],320,show_frame->data[2],320);
	SDL_RenderClear(sdlRenderer); 
	SDL_RenderCopy(sdlRenderer,sdlTexture,NULL,NULL);
	SDL_RenderPresent(sdlRenderer);
}
//-----------------------------------------------------


static int open_input_file(const char *filename)
{
    int ret;
    unsigned int i;
	AVInputFormat* input_device = NULL;
	AVDictionary *options=NULL;
	
	//�豸��ʼ��
	avdevice_register_all();
	
	//���[video input] too full or near too full (101% of size: 3041280 [rtbufsize parameter])! frame dropped!
	av_dict_set_int(&options, "rtbufsize", 3041280 * 100, 0);//Ĭ�ϴ�С3041280

	//���豸
	input_device = av_find_input_format("dshow");
	if(!input_device)
	{
		printf("av_find_input_format()\n");
		return ret;
	}	

	//������ͷ
    if ((ret = avformat_open_input(&ifmt_ctx, filename, input_device, &options)) < 0)
	{
        printf("Cannot open input file\n");
        return ret;
    }

	//��ȡ��
    if ((ret = avformat_find_stream_info(ifmt_ctx, NULL)) < 0) 
	{
        printf("Cannot find stream information\n");
        return ret;
    }

	//������Ƶ��
    for (i = 0; i < ifmt_ctx->nb_streams; i++)
	{
		AVStream *stream = ifmt_ctx->streams[i];
		
		if(stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			//��������Ӧ�Ľ����� AV_CODEC_ID_RAWVIDEOԭʼѹ����ʽ 13
			AVCodec *dec = avcodec_find_decoder(stream->codecpar->codec_id);
			if (!dec) 
			{
				printf("Failed to find decoder for stream #%u\n", i);
				return AVERROR_DECODER_NOT_FOUND;
			}			
			
			//���ݽ��������������ctx
			AVCodecContext *codec_ctx = avcodec_alloc_context3(dec);
			if (!codec_ctx)
			{
				printf("Failed to allocate the decoder context for stream #%u\n", i);
				return AVERROR(ENOMEM);
			}
			
			//copy���������ݵ�������ctx
			ret = avcodec_parameters_to_context(codec_ctx, stream->codecpar);
			if (ret < 0)
			{
				printf("Failed to copy decoder parameters to input decoder context for stream #%u\n", i);
				return ret;
			}
			printf("pix_fmt:%d\n", codec_ctx->pix_fmt);
			
			//��Ƶ����ȡһ��֡��,�����Ҫ�Ļ�
			codec_ctx->framerate = av_guess_frame_rate(ifmt_ctx, stream, NULL);
			
			//�򿪽�����
            ret = avcodec_open2(codec_ctx, dec, NULL);
            if (ret < 0) 
			{
                printf("Failed to open decoder for stream #%u\n", i);
                return ret;
            }
			
			//���������ctx
			dec_ctx = codec_ctx;

			//��ӡʱ���֡�������Ϣ
			printf("===========input===========\n");
			printf("AVFormatContext:\n");
			printf("1.AV_TIME_BASE:%lld\n", AV_TIME_BASE);
			printf("2.AV_TIME_BASE_Q:%d/%ld\n", AV_TIME_BASE_Q.num, AV_TIME_BASE_Q.den);
			printf("3.start_time:%lld\n", ifmt_ctx->start_time);
			printf("4.duration:%ld\n", ifmt_ctx->duration);
			printf("5.bit_rate:%ld\n", ifmt_ctx->bit_rate);
			
			printf("AVStream:\n");
			printf("1.time_base:%d/%ld\n", stream->time_base.num, stream->time_base.den);
			printf("2.start_time:%lld\n", stream->start_time);
			printf("3.duration:%ld\n", stream->duration);

			printf("dec_ctx:\n");
			printf("1.time_base:%d/%ld\n", dec_ctx->time_base.num, dec_ctx->time_base.den);
			printf("2.pix_fmt:%d\n", dec_ctx->pix_fmt);
			printf("===========input===========\n");
		}
	}
	
    av_dump_format(ifmt_ctx, 0, filename, 0);
    return 0;
}

static int open_output_file(const char *filename)
{
    int ret;
    unsigned int i;

	//������ļ�
    avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, filename);
    if (!ofmt_ctx) 
	{
        printf("Could not create output context\n");
        return AVERROR_UNKNOWN;
    }

	//���������
    for (i = 0; i < ifmt_ctx->nb_streams; i++)
	{
		AVStream *stream = ifmt_ctx->streams[i];
		
		if(stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
		{	
			//����һ�������
			AVStream *out_stream = avformat_new_stream(ofmt_ctx, NULL);
			if (!out_stream) 
			{
				printf("Failed allocating output stream\n");
				return AVERROR_UNKNOWN;
			}
			
			//������������óɺͽ�����һ��,Ҳ����ָ��������,AV_CODEC_ID_H264
			AVCodec *encoder = avcodec_find_encoder(AV_CODEC_ID_H264);
			if (!encoder) 
			{
				printf("Necessary encoder not found\n");
				return AVERROR_INVALIDDATA;
			}
			//���������ctx
			AVCodecContext *codec_ctx = avcodec_alloc_context3(encoder);
			if (!codec_ctx) 
			{
				printf("Failed to allocate the encoder context\n");
				return AVERROR(ENOMEM);
			}

			//���������һЩ�������óɺ������ļ�һ��
			codec_ctx->height = dec_ctx->height;
			codec_ctx->width = dec_ctx->width;
	
			//PAR ���� Pixel Aspect Ratio ���غ��ݱȡ���ʾÿ�����صĿ���볤�ȵı�ֵ��������Ϊÿ�����ز��������εġ�
			//DAR ���� Display Aspect Ratio ��ʾ���ݱȡ�������ʾ��ͼ���ڳ��ȵ�λ�ϵĺ��ݱȡ�
			//SAR ���� Sample Aspect Ratio �������ݱȡ���ʾ��������ص�������������ص����ı�ֵ��
			codec_ctx->sample_aspect_ratio = dec_ctx->sample_aspect_ratio;
			printf("pix-x:%d  pix-y:%d\n", codec_ctx->sample_aspect_ratio.num, codec_ctx->sample_aspect_ratio.den);
	
			//��������ظ�ʽ,������������ĸ�ʽ��ͬ,����Ҫת��ʽ
			codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
		
			//���ݽ�������֡�������������ʱ���
			codec_ctx->time_base = av_inv_q(dec_ctx->framerate);
			
			//�ļ�Ԥ����־��������ʱû�о�
			if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
				codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

			//�򿪱�����,����������һЩ��ʼ���Ĺ���
			ret = avcodec_open2(codec_ctx, encoder, NULL);
			if (ret < 0) 
			{
				printf("Cannot open video encoder for stream #%u\n", i);
				return ret;
			}
			
			//��ʼ��������
			ret = avcodec_parameters_from_context(out_stream->codecpar, codec_ctx);
			if (ret < 0) 
			{
				printf("Failed to copy encoder parameters to output stream #%u\n", i);
				return ret;
			}
			
			//�����ʱ�����ֵ
			out_stream->time_base = codec_ctx->time_base;
			
			//������B֡,���ڴ���pts��dts
			codec_ctx->max_b_frames = 0;
			
			//���������ctx
			enc_ctx = codec_ctx;
			
			//��ӡʱ���֡�������Ϣ
			printf("===========output===========\n");
			printf("AVFormatContext:\n");
			printf("1.AV_TIME_BASE:%lld\n", AV_TIME_BASE);
			printf("2.AV_TIME_BASE_Q:%d/%ld\n", AV_TIME_BASE_Q.num, AV_TIME_BASE_Q.den);
			printf("3.start_time:%lld\n", ofmt_ctx->start_time);
			printf("4.duration:%ld\n", ofmt_ctx->duration);
			printf("5.bit_rate:%lld\n", ofmt_ctx->bit_rate);
			
			printf("out_stream:\n");
			printf("1.time_base:%d/%ld\n", out_stream->time_base.num, stream->time_base.den);
			printf("2.start_time:%lld\n", out_stream->start_time);
			printf("3.duration:%ld\n", out_stream->duration);

			printf("enc_ctx:\n");
			printf("1.time_base:%d/%ld\n", enc_ctx->time_base.num, enc_ctx->time_base.den);
			printf("2.pix_fmt:%d\n", enc_ctx->pix_fmt);
			printf("3.max_b_frames:%d\n", enc_ctx->max_b_frames);
			printf("===========output===========\n");
		}
	}
    av_dump_format(ofmt_ctx, 0, filename, 1);

	//������ļ�
    if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) 
	{
        ret = avio_open(&ofmt_ctx->pb, filename, AVIO_FLAG_WRITE);
        if (ret < 0) 
		{
            printf("Could not open output file '%s'", filename);
            return ret;
        }
    }

    //д�ļ�ͷ
    ret = avformat_write_header(ofmt_ctx, NULL);
    if (ret < 0) 
	{
        printf("Error occurred when opening output file\n");
        return ret;
    }

    return 0;
}


static int encode_write_frame(AVFrame *sws_frame, unsigned int stream_index, int *got_frame) 
{
    int ret;
    int got_frame_local;
    AVPacket enc_pkt;
	static unsigned long long last_pts=0;
	static unsigned long long last_dts=0;
	static run_once = 1; 
	
    if (!got_frame)
        got_frame = &got_frame_local;

    printf("Encoding frame\n");
	
    
    enc_pkt.data = NULL;
    enc_pkt.size = 0;
    av_init_packet(&enc_pkt);
    
//	sws_frame->pts = last_pts;
//	sws_frame->pkt_dts = last_dts;
	
//	last_pts++;
//	last_dts++;

#if 0	
	if(sws_frame->pts > last_pts)
	{
		last_pts = sws_frame->pts;
	}
	else
	{
		sws_frame->pts  = last_pts++;
		last_pts = sws_frame->pts;
	}	
#endif	
	
	//��ʼ����
    ret = avcodec_encode_video2(enc_ctx, &enc_pkt, sws_frame, got_frame);
    av_frame_free(&sws_frame);
    if (ret < 0)
        return ret;
    if (!(*got_frame))
        return 0;
	
	printf("����ʱ���:time_base:%d/%ld\n", enc_ctx->time_base.num, enc_ctx->time_base.den);
	printf("�����İ�:pts=%lld\n", enc_pkt.pts);
	printf("�����İ�:dts=%lld\n", enc_pkt.dts);
	printf("�����İ�:duration=%lld\n", enc_pkt.duration);
	printf("----------------------------------\n");
	
    //��������Ӧ��index
    enc_pkt.stream_index = stream_index;
	
	//ʱ���ת��enc_ctx.time_base---->ofmt_ctx.time_base
    av_packet_rescale_ts(&enc_pkt, enc_ctx->time_base, ofmt_ctx->streams[stream_index]->time_base);
	printf("�����ʱ���:time_base:%d/%ld\n", ofmt_ctx->streams[stream_index]->time_base.num, ofmt_ctx->streams[stream_index]->time_base.den);
	printf("ת�������İ�:pts=%lld\n", enc_pkt.pts);
	printf("ת�������İ�:dts=%lld\n", enc_pkt.dts);
	printf("ת�������İ�:duration=%lld\n", enc_pkt.duration);
	printf("--------------end--------------------\n");	

//��ϴ������,���Application provided invalid, non monotonically increasing dts
//to muxer in stream 0: 123518876481 >= 123518876481

//	enc_pkt.pts = last_pts;
//	enc_pkt.dts = last_dts;
	
//	last_pts++;
//	last_dts++;
	
#if 0
	if(enc_pkt.dts > last_dts)
	{
		last_dts = enc_pkt.dts;
	}
	else
	{
		enc_pkt.dts  = last_dts++;
		last_dts = enc_pkt.dts;
	}	
#endif	


    printf("Muxing frame\n");
	
    //д�ļ�,��ʼ��װ
    ret = av_interleaved_write_frame(ofmt_ctx, &enc_pkt);
    return ret;
}


static int swscale_encode_write_frame(AVFrame *frame, unsigned int stream_index)
{
	AVFrame *sws_frame=NULL;
	
	//ת��ʽ
	struct SwsContext *img_convert_ctx = sws_getContext(640,480,AV_PIX_FMT_YUYV422,640,480,AV_PIX_FMT_YUV420P,SWS_BILINEAR,NULL,NULL,NULL);
	
	//����ռ�
	sws_frame = av_frame_alloc();
	unsigned char *sws_buf=(unsigned char *)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_YUV420P,640,480,4));  	
	av_image_fill_arrays((AVPicture*)sws_frame,sws_frame->linesize,sws_buf,AV_PIX_FMT_YUV420P,640,480,4);
	
	//ת��ʽ
	sws_scale(img_convert_ctx,
			(const unsigned char* const*)frame->data,
			frame->linesize,
			0,
			480,
			sws_frame->data,
			sws_frame->linesize);
	
	//��ʾ
	sdl_show(sws_frame);
#if 0
	printf("ת��ʽ���֡:pts=%lld\n", sws_frame->pts);
	printf("ת��ʽ���֡:dts=%lld\n", sws_frame->pkt_dts);
	printf("ת��ʽ���֡:duration=%lld\n", sws_frame->pkt_duration);
	printf("ת��ʽ���֡:best_effort_timestamp=%d\n", sws_frame->best_effort_timestamp);
	printf("ת��ʽ���֡:format=%d\n", sws_frame->format);
	printf("ת��ʽ���֡:width=%d\n", sws_frame->width);
	printf("ת��ʽ���֡:height=%d\n", sws_frame->height);
	printf("----------------------------------\n");	
#endif
	sws_frame->pts = frame->pts;
	sws_frame->pkt_dts = frame->pkt_dts;
	sws_frame->pkt_duration = frame->pkt_duration;
	sws_frame->best_effort_timestamp = frame->best_effort_timestamp;
	sws_frame->format = AV_PIX_FMT_YUV420P;
	sws_frame->width = frame->width;
	sws_frame->height = frame->height;
	sws_frame->pict_type = frame->pict_type;

	printf("��ֵ���֡:pts=%lld\n", sws_frame->pts);
	printf("��ֵ���֡:dts=%lld\n", sws_frame->pkt_dts);
	printf("��ֵ���֡:duration=%lld\n", sws_frame->pkt_duration);
	printf("��ֵ���֡:best_effort_timestamp=%d\n", sws_frame->best_effort_timestamp);
	printf("��ֵ���֡:format=%d\n", sws_frame->format);
	printf("��ֵ���֡:width=%d\n", sws_frame->width);
	printf("��ֵ���֡:height=%d\n", sws_frame->height);
	printf("��ֵ���֡��ʽ:type=%d\n", sws_frame->pict_type);
	printf("----------------------------------\n");	
	
	encode_write_frame(sws_frame, stream_index, NULL);

	av_free(sws_buf);
	sws_freeContext(img_convert_ctx);
	
	return 0;	
}

static int flush_encoder(unsigned int stream_index)
{
    int ret;
    int got_frame;

    if (!(enc_ctx->codec->capabilities & AV_CODEC_CAP_DELAY))
        return 0;

    while (1) 
	{
        printf("Flushing stream #%u encoder\n", stream_index);
        ret = encode_write_frame(NULL, stream_index, &got_frame);
        if (ret < 0)
            break;
        if (!got_frame)
            return 0;
    }
    return ret;
}

int main(int argc, char **argv)
{
    int ret;
    AVPacket packet = { .data = NULL, .size = 0 };
    AVFrame *frame = NULL;
    enum AVMediaType type;
    unsigned int stream_index;
    unsigned int i;
    int got_frame;
	static int64_t last_packet_pts=0;

	sdl_windows_init();
	

    if ((ret = open_input_file("video=HP Webcam-101")) < 0)
        goto end;
    if ((ret = open_output_file("2.mp4")) < 0)
        goto end;
	
	//system("pause");
	//enc_ctx->time_base.num = dec_ctx->time_base.num;
	//enc_ctx->time_base.den = dec_ctx->time_base.den;
	//enc_ctx->framerate.num = dec_ctx->framerate.num;
	//enc_ctx->framerate.den = dec_ctx->framerate.den;

	unsigned int count = 300;
    while(count--)
	{
        if((ret = av_read_frame(ifmt_ctx, &packet)) < 0)
            break;
		
        stream_index = packet.stream_index;
        type = ifmt_ctx->streams[stream_index]->codecpar->codec_type;

		if(type == AVMEDIA_TYPE_VIDEO)
		{
            frame = av_frame_alloc();
            if (!frame) 
			{
                ret = AVERROR(ENOMEM);
                break;
            }
			
			//�ж϶�ȡ����packet��pts��dtsʱ����ǲ���һ���� 
			if(packet.pts != packet.dts)
			{
				printf("aaaaaaaaaaaaaaaaaaa");
				system("pause");
			}
			
			//ʹ��AVStream�����time_base=1/1000 0000
			printf("----------------start------------------\n");
			printf("������ʱ���:%d/%d\n", ifmt_ctx->streams[stream_index]->time_base.num,ifmt_ctx->streams[stream_index]->time_base.den);
			printf("��ȡ�İ�:pts=%lld\n", packet.pts);
			printf("��ȡ�İ�:dts=%lld\n", packet.dts);
			printf("��ȡ�İ�:duration=%lld\n", packet.duration);
			printf("----------------------------------\n");
			
			//��ifmt_ctx�������ʱ���ת��codec�������ʱ���
            av_packet_rescale_ts(&packet, ifmt_ctx->streams[stream_index]->time_base, dec_ctx->time_base);
			
			printf("������ʱ���:%d/%d\n", dec_ctx->time_base.num, dec_ctx->time_base.den);
			printf("ת����:pts=%lld\n", packet.pts);
			printf("ת����:dts=%lld\n", packet.dts);
			printf("ת����:duration=%lld\n", packet.duration);
			printf("----------------------------------\n");	

			//��ʼ����
            ret = avcodec_decode_video2(dec_ctx, frame, &got_frame, &packet);
            if (ret < 0)
			{
                av_frame_free(&frame);
                printf("Decoding failed\n");
                break;
            }
			//pts pkt_dts pkt_duration ==copy��packet, pkt_duration����AVStreamʱ���������
			printf("�����ʱ���:%d/%d\n", dec_ctx->time_base.num, dec_ctx->time_base.den);
			printf("�����֡:pts=%lld\n", frame->pts);
			printf("�����֡:dts=%lld\n", frame->pkt_dts);
			printf("�����֡:duration=%lld\n", frame->pkt_duration);
			printf("�����֡:best_effort_timestamp=%d\n", frame->best_effort_timestamp);
			printf("�����֡:format=%d\n", frame->format);
			printf("�����֡:width=%d\n", frame->width);
			printf("�����֡:height=%d\n", frame->height);
			printf("�����֡��ʽ:type=%d\n", frame->pict_type);
			printf("----------------------------------\n");	
			
			//�鿴������û��B֡
			if(frame->pict_type == AV_PICTURE_TYPE_B || frame->pict_type == AV_PICTURE_TYPE_BI)
			{
	
				printf("bbbbbbbbbbbbbbbb");
				system("pause");
			}
			
			//����ɹ�
            if(got_frame)
			{
				//����дһ��frame pts,Ȼ���͵�������
                //frame->pts = frame->best_effort_timestamp;
				swscale_encode_write_frame(frame, stream_index);
				
                av_frame_free(&frame);
                if (ret < 0)
				goto end;
            } 
			else 
			{
                av_frame_free(&frame);
            }
        } 
        av_packet_unref(&packet);
    }
#if 1
    //��ϴ������
    ret = flush_encoder(stream_index);
    if (ret < 0)
	{
        printf("Flushing encoder failed\n");
        goto end;
    }
#endif    
	//д����ļ�
    av_write_trailer(ofmt_ctx);
	
end:
#if 0
    av_packet_unref(&packet);
    av_frame_free(&frame);
    for (i = 0; i < ifmt_ctx->nb_streams; i++) 
	{
        avcodec_free_context(&stream_ctx[i].dec_ctx);
        if (ofmt_ctx && ofmt_ctx->nb_streams > i && ofmt_ctx->streams[i] && stream_ctx[i].enc_ctx)
            avcodec_free_context(&stream_ctx[i].enc_ctx);
        if (filter_ctx && filter_ctx[i].filter_graph)
            avfilter_graph_free(&filter_ctx[i].filter_graph);
    }
    av_free(filter_ctx);
    av_free(stream_ctx);
    avformat_close_input(&ifmt_ctx);
    if (ofmt_ctx && !(ofmt_ctx->oformat->flags & AVFMT_NOFILE))
        avio_closep(&ofmt_ctx->pb);
    avformat_free_context(ofmt_ctx);

    if (ret < 0)
        printf( "Error occurred: %s\n", av_err2str(ret));
#endif
    return ret ? 1 : 0;

}


