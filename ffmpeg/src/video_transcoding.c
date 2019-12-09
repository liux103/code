

/*				������ת������ʱ���							   ת�ɱ�����ʱ���								ת�������ʱ���
��ȡһ��packet------------------------>���������--->���frame--->------------------->���������--->���packet-------------------->д�ļ�
*/


 
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include "libswscale/swscale.h"

unsigned long int index1=0;
unsigned long int index2=0;

static AVFormatContext *ifmt_ctx=NULL;
static AVFormatContext *ofmt_ctx=NULL;

static AVCodecContext *dec_ctx=NULL;
static AVCodecContext *enc_ctx=NULL;

#include <windows.h>
FILE* log_fp = NULL;


//-----------------------------------------------------

#define WIDTH 1280
#define HEIGTH 720
#define PIX_FMT SDL_PIXELFORMAT_IYUV

#include <SDL2/SDL.h>
SDL_Window* sdlscreen=NULL;
SDL_Renderer* sdlRenderer=NULL;
SDL_Texture* sdlTexture=NULL;


int sdl_windows_init()
{
    SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER);
    sdlscreen = SDL_CreateWindow("windows", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WIDTH, HEIGTH, SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);
    sdlRenderer=SDL_CreateRenderer(sdlscreen, -1, 0);
	//SDL_PIXELFORMAT_YUY2  SDL_PIXELFORMAT_YV12  SDL_PIXELFORMAT_BGR888 SDL_PIXELFORMAT_YV12 SDL_PIXELFORMAT_IYUV
    sdlTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING,WIDTH,HEIGTH);
	
    return 0;
}

int sdl_show(AVFrame *show_frame)
{
	SDL_UpdateYUVTexture(sdlTexture,NULL,show_frame->data[0],WIDTH,show_frame->data[1],WIDTH/2,show_frame->data[2],WIDTH/2);
	SDL_RenderClear(sdlRenderer); 
	SDL_RenderCopy(sdlRenderer,sdlTexture,NULL,NULL);
	SDL_RenderPresent(sdlRenderer);
}
//-----------------------------------------------------


static int open_input_file(const char *filename)
{
    int ret;
    unsigned int i;

	//���ļ�
    if ((ret = avformat_open_input(&ifmt_ctx, filename, NULL, NULL)) < 0)
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

	//������
    for (i = 0; i < ifmt_ctx->nb_streams; i++)
	{
		AVStream *stream = ifmt_ctx->streams[i];
		
		//����ֻ������Ƶ��
		if(stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			//��������Ӧ�Ľ�����
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
			
			//��Ƶ����ȡһ��֡��,�����Ҫ�Ļ�
			codec_ctx->framerate = av_guess_frame_rate(ifmt_ctx, stream, NULL);
			
			//�򿪽�����
            ret = avcodec_open2(codec_ctx, dec, NULL);
            if (ret < 0) 
			{
                printf("Failed to open decoder for stream #%u\n", i);
                return ret;
            }
			
			//���������ctx  bit_rate
			dec_ctx = codec_ctx;
			
			fprintf(log_fp, "1.����ctxʱ���:%ld/%ld\n", AV_TIME_BASE_Q.num, AV_TIME_BASE_Q.den);
			fprintf(log_fp, "2.����ctx��ʼʱ��:%ld\n", ifmt_ctx->start_time);
			fprintf(log_fp, "3.����ctx����ʱ��:%ld\n", ifmt_ctx->duration);
			fprintf(log_fp, "4.����ctx������:%ld\n", ifmt_ctx->bit_rate);
			fprintf(log_fp, "5.������Ƶ��ʱ���:%ld/%ld\n", stream->time_base.num, stream->time_base.den);
			fprintf(log_fp, "6.������Ƶ������ʱ��:%ld\n", stream->duration);
			fprintf(log_fp, "7.������ʱ���:%ld/%ld\n", dec_ctx->time_base.num, dec_ctx->time_base.den);
			fprintf(log_fp, "8.���������� :%ld\n", dec_ctx->bit_rate);
			fprintf(log_fp, "9.��Ƶ֡��:%d\n", dec_ctx->framerate);
			fprintf(log_fp, "10.���ظ�ʽ:%d\n", dec_ctx->pix_fmt);
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
		//������
		AVStream *stream = ifmt_ctx->streams[i];
		
		if(stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
		{	
			//Ϊofmt_ctx����һ����Ƶ��������
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
	
			//��������ظ�ʽ,������������ĸ�ʽ��ͬ,����Ҫת��ʽ
			codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
		
			//���ݽ�������֡�������������ʱ���,����������ʱ���һ��,������һ��ʱ���ת��
			//codec_ctx->time_base = av_inv_q(dec_ctx->framerate);
			codec_ctx->time_base = dec_ctx->time_base;
			
			//��������һ��(ʵ�ʻ�ȡ��),�������ʻή����Ƶ����,��ʧ������һ�������ݣ�����
			codec_ctx->bit_rate = dec_ctx->bit_rate;

			//������B֡,���ڴ���pts��dts
			codec_ctx->max_b_frames = 0;
			
			//�������֡��һ��
			codec_ctx->framerate = dec_ctx->framerate;
			
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
			
			//���������ʱ������ó�һ��
			out_stream->time_base = stream->time_base;
			
			//�������������ʱ������ó�һ��
			out_stream->duration = stream->duration;
			
			//��ʼ��������
			ret = avcodec_parameters_from_context(out_stream->codecpar, codec_ctx);
			if (ret < 0) 
			{
				printf("Failed to copy encoder parameters to output stream #%u\n", i);
				return ret;
			}
			
			//���������ctx
			enc_ctx = codec_ctx;
			
			fprintf(log_fp, "1.���ctxʱ���:%ld/%ld\n", AV_TIME_BASE_Q.num, AV_TIME_BASE_Q.den);
			fprintf(log_fp, "2.���ctx��ʼʱ��:%ld\n", ofmt_ctx->start_time);
			fprintf(log_fp, "3.���ctx����ʱ��:%ld\n", ofmt_ctx->duration);
			fprintf(log_fp, "4.���ctx������:%ld\n", ofmt_ctx->bit_rate);
			fprintf(log_fp, "5.�����Ƶ��ʱ���:%ld/%ld\n", out_stream->time_base.num, out_stream->time_base.den);
			fprintf(log_fp, "6.�����Ƶ������ʱ��:%ld\n", out_stream->duration);
			fprintf(log_fp, "7.������ʱ���:%ld/%ld\n", enc_ctx->time_base.num, enc_ctx->time_base.den);
			fprintf(log_fp, "8.���������� :%ld\n", enc_ctx->bit_rate);
			fprintf(log_fp, "9.��Ƶ֡��:%d\n", enc_ctx->framerate);
			fprintf(log_fp, "10.���ظ�ʽ:%d\n", enc_ctx->pix_fmt);		
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
    AVPacket enc_pkt = { .data = NULL, .size = 0 };
	
	
	if (!got_frame)
        got_frame = &got_frame_local;
	
    printf("Encoding frame\n");
	
    av_init_packet(&enc_pkt);
    
	//��ʼ����
    ret = avcodec_encode_video2(enc_ctx, &enc_pkt, sws_frame, got_frame);
    av_frame_free(&sws_frame);
    if (ret < 0)
        return ret;
    if (!(*got_frame))
        return 0;
	
	//printf("index2:%d\n", ++index2);
	
	fprintf(log_fp, "����ʱ���:time_base:%d/%ld\n", enc_ctx->time_base.num, enc_ctx->time_base.den);
	fprintf(log_fp, "�����İ�:pts=%lld\n", enc_pkt.pts);
	fprintf(log_fp, "�����İ�:dts=%lld\n", enc_pkt.dts);
	fprintf(log_fp, "�����İ�:duration=%lld\n", enc_pkt.duration);
	fprintf(log_fp, "----------------------------------\n");
	
    //��������Ӧ��index
    enc_pkt.stream_index = stream_index;
	
	//ʱ���ת��enc_ctx.time_base---->ofmt_ctx.time_base
    av_packet_rescale_ts(&enc_pkt, enc_ctx->time_base, ofmt_ctx->streams[stream_index]->time_base);
	fprintf(log_fp, "�����ʱ���:time_base:%d/%ld\n", ofmt_ctx->streams[stream_index]->time_base.num, ofmt_ctx->streams[stream_index]->time_base.den);
	fprintf(log_fp, "ת�������İ�:pts=%lld\n", enc_pkt.pts);
	fprintf(log_fp, "ת�������İ�:dts=%lld\n", enc_pkt.dts);
	fprintf(log_fp, "ת�������İ�:duration=%lld\n", enc_pkt.duration);
	fprintf(log_fp, "--------------end--------------------\n");	

    printf("Muxing frame\n");
	
    //д�ļ�,��ʼ��װ
    ret = av_interleaved_write_frame(ofmt_ctx, &enc_pkt);
    return ret;
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

	//sdl_windows_init();
	
	log_fp = fopen("log.txt", "wb+");
	
    if ((ret = open_input_file("1.mp4")) < 0)
        goto end;
    if ((ret = open_output_file("2.mp4")) < 0)
        goto end;
	
	//system("pause");
	
    while(1)
	{
        if((ret = av_read_frame(ifmt_ctx, &packet)) < 0)
        break;
		
        stream_index = packet.stream_index;
        type = ifmt_ctx->streams[stream_index]->codecpar->codec_type;
		frame = av_frame_alloc();

		if(type == AVMEDIA_TYPE_VIDEO)
		{
			//�ж϶�ȡ����packet��pts��dtsʱ����ǲ���һ����,������
			if(packet.pts != packet.dts)
			{
				fprintf(log_fp, "packet.pts != packet.dts\n");
			}
		
			//ʹ��AVStream�����time_base=1/1000 0000
			fprintf(log_fp, "----------------start------------------\n");
			fprintf(log_fp, "������ʱ���:%d/%d\n", ifmt_ctx->streams[stream_index]->time_base.num,ifmt_ctx->streams[stream_index]->time_base.den);
			fprintf(log_fp, "��ȡ�İ�:pts=%lld\n", packet.pts);
			fprintf(log_fp, "��ȡ�İ�:dts=%lld\n", packet.dts);
			fprintf(log_fp, "��ȡ�İ�:duration=%lld\n", packet.duration);
			fprintf(log_fp, "----------------------------------\n");
			
			//��ifmt_ctx�������ʱ���ת��codec�������ʱ���
            av_packet_rescale_ts(&packet, ifmt_ctx->streams[stream_index]->time_base, dec_ctx->time_base);
			
			fprintf(log_fp, "������ʱ���:%d/%d\n", dec_ctx->time_base.num, dec_ctx->time_base.den);
			fprintf(log_fp, "ת����:pts=%lld\n", packet.pts);
			fprintf(log_fp, "ת����:dts=%lld\n", packet.dts);
			fprintf(log_fp, "ת����:duration=%lld\n", packet.duration);
			fprintf(log_fp, "----------------------------------\n");	

			//��ʼ����
            ret = avcodec_decode_video2(dec_ctx, frame, &got_frame, &packet);
            if (ret < 0)
			{
                printf("Decoding failed\n");
                break;
            }
			//pts pkt_dts pkt_duration ==copy��packet, pkt_duration����AVStreamʱ���������
			fprintf(log_fp, "�����ʱ���:%d/%d\n", dec_ctx->time_base.num, dec_ctx->time_base.den);
			fprintf(log_fp, "�����֡:pts=%lld\n", frame->pts);
			fprintf(log_fp, "�����֡:dts=%lld\n", frame->pkt_dts);
			fprintf(log_fp, "�����֡:duration=%lld\n", frame->pkt_duration);
			fprintf(log_fp, "�����֡:best_effort_timestamp=%d\n", frame->best_effort_timestamp);
			fprintf(log_fp, "�����֡:format=%d\n", frame->format);
			fprintf(log_fp, "�����֡:width=%d\n", frame->width);
			fprintf(log_fp, "�����֡:height=%d\n", frame->height);
			fprintf(log_fp, "I P B:type=%d\n", frame->pict_type);
			fprintf(log_fp, "----------------------------------\n");	

			//�鿴������û��B֡,������
			if(frame->pict_type == AV_PICTURE_TYPE_B || frame->pict_type == AV_PICTURE_TYPE_BI)
			{
				fprintf(log_fp, "bas B frame!!!\n");
			}
			
			//����ɹ�
            if(got_frame)
			{
				//sdl_show(frame);
				//printf("index1:%d\n", ++index1);
				
				//����дһ��frame pts,Ȼ��ʼ����
                frame->pts = frame->best_effort_timestamp;
				ret = encode_write_frame(frame, stream_index, &got_frame);
				
                if (ret < 0)
				goto end;
            }   
        } 
		av_frame_free(&frame); 
        av_packet_unref(&packet);
    }

    //��ϴ������
    ret = flush_encoder(stream_index);
    if (ret < 0)
	{
        printf("Flushing encoder failed\n");
        goto end;
    }
   
	//д����ļ�
    av_write_trailer(ofmt_ctx);
	
	
end:
	if(!log_fp)
	fclose(log_fp);

	if(!(&packet))
    av_packet_unref(&packet);

	if(!(&frame))
    av_frame_free(&frame);

	if(!(&dec_ctx))
    avcodec_free_context(&dec_ctx);

	if(!(&enc_ctx))
	avcodec_free_context(&enc_ctx);
	
	if(!(&ifmt_ctx))
    avformat_close_input(&ifmt_ctx);
	
    if(ofmt_ctx && !(ofmt_ctx->oformat->flags & AVFMT_NOFILE))
    avio_closep(&ofmt_ctx->pb);
	
	if(!ofmt_ctx)
    avformat_free_context(ofmt_ctx);

    return ret ? 1 : 0;

}

















