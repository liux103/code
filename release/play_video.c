#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"
#include "libavutil/opt.h"
#include "libavutil/pixdesc.h"
#include "libswscale/swscale.h"
#include "libavdevice/avdevice.h"
#include "windows.h"

AVFormatContext *ifmt_ctx=NULL;
AVCodecContext *dec_ctx=NULL;

//-----------------------------------------------------
#include <SDL2/SDL.h>
#define SDL_REFRESH 	SDL_USEREVENT+1
SDL_Window* sdlscreen=NULL;
SDL_Renderer* sdlRenderer=NULL;
SDL_Texture* sdlTexture=NULL;
SDL_Thread* thread_id=NULL;
SDL_Event event;
int width; 
int height;

//�����߳�
int refresh_task(void *p)
{
	SDL_Event event;
	while(1)
	{		
		event.type = SDL_REFRESH;
		SDL_PushEvent(&event);
	}
	return 0;
}

//Ŀǰֻ����yuv420
int sdl_windows_init(int w, int h)
{
	width = w;
	height = h;
	
    SDL_Init(SDL_INIT_VIDEO);
    sdlscreen = SDL_CreateWindow("windows", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);
    sdlRenderer=SDL_CreateRenderer(sdlscreen, -1, 0);
	//SDL_PIXELFORMAT_YUY2  SDL_PIXELFORMAT_YV12  SDL_PIXELFORMAT_BGR888 SDL_PIXELFORMAT_YV12 SDL_PIXELFORMAT_IYUV
    sdlTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, width, height);
	thread_id = SDL_CreateThread(refresh_task,NULL,NULL);
	
    return 0;
}

int sdl_show(AVFrame *show_frame)
{
	SDL_UpdateYUVTexture(sdlTexture,NULL,show_frame->data[0],width,show_frame->data[1],width/2,show_frame->data[2],width/2);
	SDL_RenderClear(sdlRenderer); 
	SDL_RenderCopy(sdlRenderer,sdlTexture,NULL,NULL);
	SDL_RenderPresent(sdlRenderer);
	
	return 0;
}
//-----------------------------------------------------

int open_input_file(const char *filename)
{
    int ret;
    int i=0;
	int video_index = -1;

	//������ͷ desktop filename
	printf("connect to %s...\n", filename);
    if((ret = avformat_open_input(&ifmt_ctx, filename, NULL, NULL)) < 0)
	{
        return ret;
    }

	//��ȡ��
    if((ret = avformat_find_stream_info(ifmt_ctx, NULL)) < 0)
	{
        return ret;
    }

	//������Ƶ��index
    for(i = 0; i < ifmt_ctx->nb_streams; i++)
	{
		if(ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			video_index = i;
			break;	
		}
	}
	
	if(video_index==-1)
	{
        return ret;	
	}
	
	AVStream *stream = ifmt_ctx->streams[video_index];

	//��������Ӧ�Ľ����� AV_CODEC_ID_RAWVIDEOԭʼѹ����ʽ 13
	AVCodec *dec = avcodec_find_decoder(stream->codecpar->codec_id);
	if(!dec) 
	{
		return -1;
	}			
	
	//���ݽ��������������ctx
	AVCodecContext *codec_ctx = avcodec_alloc_context3(dec);
	if (!codec_ctx)
	{
		return -1;
	}
	
	//copy���������ݵ�������ctx
	ret = avcodec_parameters_to_context(codec_ctx, stream->codecpar);
	if(ret < 0)
	{
		return ret;
	}

	//��Ƶ����ȡһ��֡��,�����Ҫ�Ļ�
	codec_ctx->framerate = av_guess_frame_rate(ifmt_ctx, stream, NULL);
	
	//�򿪽�����
    ret = avcodec_open2(codec_ctx, dec, NULL);
    if(ret < 0) 
	{
        return ret;
    }
	
	//���������ctx
	dec_ctx = codec_ctx;
	
	printf("in stream info:\n");
	printf("1.bit_rate:%ld\n", ifmt_ctx->bit_rate);
	printf("2.time_base:%d/%ld\n", stream->time_base.num, stream->time_base.den);
	printf("3.start_time:%lld\n", stream->start_time);
	printf("decode info:\n");
	printf("1.time_base:%d/%ld\n", dec_ctx->time_base.num, dec_ctx->time_base.den);
	printf("2.pix_fmt:%d\n", dec_ctx->pix_fmt);
	printf("3.decodec_id:%d\n", stream->codecpar->codec_id);
	printf("4.width:%d\n", dec_ctx->width);
	printf("5.height:%d\n", dec_ctx->height);
	
    return 0;
}

int main(int argc, char *argv[])
{
	AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
	int ret;
    unsigned int stream_index;
    int got_frame;
	const char *input_file = "rtmp://169.254.96.183:1935/live/home";
	unsigned int frame_index=0;

	//�����ʼ��
	avformat_network_init();

	//���ļ�
    open_input_file(input_file);
	
	//SDL��ʼ��
	sdl_windows_init(dec_ctx->width,dec_ctx->height);

    while(1)
	{
		//������SDL�߳̿���,���Կ��Ʋ�������
		SDL_WaitEvent(&event);
		if(event.type!=SDL_REFRESH)
		continue;
		
        ret = av_read_frame(ifmt_ctx, packet);
		if(ret<0)
		{
			break;
		}

        stream_index = packet->stream_index;

		if(ifmt_ctx->streams[stream_index]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			//תʱ���(in_stream--->dec_ctx)
            av_packet_rescale_ts(packet, ifmt_ctx->streams[stream_index]->time_base, dec_ctx->time_base);
			
			//��ʼ����
            ret = avcodec_decode_video2(dec_ctx, frame, &got_frame, packet);
            if(ret < 0)
			{
				//����ʧ��
                break;
            }

			//����
			if(got_frame)
			{
				sdl_show(frame);
				printf("                \rframe index:%ld\r", ++frame_index);
			}
        }
		av_frame_unref(frame);
        av_packet_unref(packet);
    }

	av_frame_free(&frame);
    av_packet_free(&packet);
	avformat_close_input(&ifmt_ctx);

    return 0;
}


