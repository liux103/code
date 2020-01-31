#include <inttypes.h>
#include <math.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>

#include "libavutil/avstring.h"
#include "libavutil/eval.h"
#include "libavutil/mathematics.h"
#include "libavutil/pixdesc.h"
#include "libavutil/imgutils.h"
#include "libavutil/dict.h"
#include "libavutil/parseutils.h"
#include "libavutil/samplefmt.h"
#include "libavutil/avassert.h"
#include "libavutil/time.h"
#include "libavformat/avformat.h"
#include "libavdevice/avdevice.h"
#include "libswscale/swscale.h"
#include "libavutil/opt.h"
#include "libavcodec/avfft.h"
#include "libswresample/swresample.h"
#include <SDL2/SDL.h>

#define MAX_FRAME_QUEUE_SIZE 32

//--------------------------------------------------------------------

//packet������нڵ�
typedef struct MyAVPacketList
{
	AVPacket pkt;
	struct MyAVPacketList *next;
} MyAVPacketList;

//packet���нṹ������ֻ�������׽ڵ��β�ڵ�,�м�ڵ�
//ͨ��MyAVPacketList->next��������,packet�����ʱ�Ͳ�
//�뵽����β��,packet������ʱ�ʹ��ײ�ȡpacket
typedef struct PacketQueue
{
	MyAVPacketList *first_pkt;	//�׽ڵ�
	MyAVPacketList *last_pkt;	//β�ڵ�
	int abort_request;			//�ж�����
	int nb_packets;				//�����ܹ�packet����
	int size;					//����packet��size��С
	SDL_mutex *mutex;			//��
	SDL_cond *cond;				//��������
} PacketQueue;

//packet���г�ʼ��
int packet_queue_init(PacketQueue *q)
{
	//��ʼ���ṹ���Ա
	memset(q, 0, sizeof(PacketQueue));

	//��Ϊpacket���лᱻ����̷߳���
	//����������Ҫ��������������Դ
	q->mutex = SDL_CreateMutex();
	if(!q->mutex)
	{
		return -1;
	}
	//��������
	q->cond = SDL_CreateCond();
	if(!q->mutex)
	{
		return -1;
	}

	return 0;
}

//packet�����
int packet_queue_put(PacketQueue *q, AVPacket *pkt)
{
	SDL_LockMutex(q->mutex);

	//1.�ȹ���һ���ڵ�,����ڵ���Ҫ���뵽����β��
	MyAVPacketList *pkt1;
	pkt1=av_malloc(sizeof(MyAVPacketList));
	if(!pkt1)
	{
		return -1;
	}
	pkt1->pkt = *pkt;
	pkt1->next = NULL;

	//2.������߼���Ҫ����ľ��ǰ��µĽڵ���ص���ǰ
	//β�ڵ��next��ȥ,����������.Ȼ����3���
	//PacketQueue��β�ڵ����Ϊ���ι���Ľڵ�
	if(!q->last_pkt)
		q->first_pkt = pkt1;
	else
		q->last_pkt->next = pkt1;

	//3.����PacketQueue��β�ڵ�Ϊ�ոչ���Ľڵ�
	q->last_pkt = pkt1;

	//4.packet������һ
	q->nb_packets++;
	
	//5.packet��С����(���������ڴ�ռ��ͳ��)
	q->size += pkt1->pkt.size + sizeof(*pkt1);

	//6.��һ��packet�������,����֪ͨ��Щ�ȴ�
	//packet���߳̿��ԴӶ���ȡpacket��
	SDL_CondSignal(q->cond);

	SDL_UnlockMutex(q->mutex);

	return 0;
}

//packet�հ������
int packet_queue_put_nullpacket(PacketQueue *q, int stream_index)
{
	//����հ���Ϊ���ļ�����ʱ,�����������Ϳհ�,������ϴ������
	AVPacket pkt1, *pkt = &pkt1;
	av_init_packet(pkt);
	pkt->data = NULL;
	pkt->size = 0;
	pkt->stream_index = stream_index;
	return packet_queue_put(q, pkt);
}

//packet������
int packet_queue_get(PacketQueue *q, AVPacket *pkt)
{
	MyAVPacketList *pkt1;
	int ret;

	SDL_LockMutex(q->mutex);	
	for (;;)
	{
		//1.ȡ�׽ڵ�	
		pkt1 = q->first_pkt;

		//������packet
		if (pkt1)
		{
			//2.�Ѷ����׽ڵ����һ��
			q->first_pkt = pkt1->next;
			if (!q->first_pkt)
				q->last_pkt = NULL;

			//3.��������һ
			q->nb_packets--;
			//4.packet��С����
			q->size -= pkt1->pkt.size + sizeof(*pkt1);
			//5.ȡpacket
			*pkt = pkt1->pkt;
			//6.�ͷ���Դ
			av_free(pkt1);
			ret = 1;
			break;
		}
		else
		{
			//�������û�а�,�����һֱ����
			SDL_CondWait(q->cond, q->mutex);
		}
	}
	SDL_UnlockMutex(q->mutex);
	return ret;
}

//packet����,�����������ƶ��г���
//��ֹ���޻�����ڴ�ľ�
int packet_queue_num(PacketQueue *q)
{
	return q->nb_packets;
}

//packet����
void packet_queue_destroy(PacketQueue *q)
{
	MyAVPacketList *pkt, *pkt1;

	SDL_LockMutex(q->mutex);
	for (pkt = q->first_pkt; pkt; pkt = pkt1)
	{
		pkt1 = pkt->next;
		av_packet_unref(&pkt->pkt);
		av_freep(&pkt);
	}
	q->last_pkt = NULL;
	q->first_pkt = NULL;
	q->nb_packets = 0;
	q->size = 0;
	SDL_UnlockMutex(q->mutex);

	SDL_DestroyMutex(q->mutex);
	SDL_DestroyCond(q->cond);
}
//--------------------------------------------------------------------

//frame�ڵ�
typedef struct Frame
{
	AVFrame *frame;	//frame
	int frame_quit;	//��Ǹ�frame�Ƿ������һ��frame
} Frame;

typedef struct FrameQueue
{
	Frame queue[MAX_FRAME_QUEUE_SIZE];//����������ʾ�ڵ�
	int rindex;			//��ǵ�ǰ�ɶ���Frame��index
	int windex;			//��ǵ�ǰ��д��Frame��index
	int size;			//��ǵ�ǰ�����Ѿ�д�˶��ٸ�
	int max_size;		//�������ڵ���
	SDL_mutex *mutex;	//��
	SDL_cond *cond;		//��������
} FrameQueue;

//frame��ʼ��
int frame_queue_init(FrameQueue *f, int max_size)
{
	int i;
	//��ʼ����Ա
	memset(f, 0, sizeof(FrameQueue));

	//������
	f->mutex = SDL_CreateMutex();
	if(!f->mutex)
	{
		return -1;
	}
	//��������
	f->cond = SDL_CreateCond();
	if(!f->cond)
	{
		return -1;
	}
	//���ڵ���
	f->max_size = max_size;
	
	//��������еĽڵ�
	for(i = 0; i < f->max_size; i++)
	{
		f->queue[i].frame = av_frame_alloc();
		if(!f->queue[i].frame)
		{
			return -1;
		}
	}

	return 0;
}

//д����ǰӦ�õ��ô˺���������һ������д�Ľڵ�
Frame *frame_queue_peek_writable(FrameQueue *f)
{
	SDL_LockMutex(f->mutex);
	
	//�������ǰ�Ѿ�д����еĸ����������ڵ���
	//˵����ǰ�����Ѿ�����,û��λ��д��,�������
	//�¾͵ȴ�,ֱ�����пճ�λ����
	while(f->size >= f->max_size)
	{
		SDL_CondWait(f->cond, f->mutex);
	}
	SDL_UnlockMutex(f->mutex);

	//����ֻ����һ����д��frame�ڵ�,��û��д����
	return &f->queue[f->windex];
}

//д������Ӧ�õ��ô˺����ѽڵ�windex���ƫ��һ��
void frame_queue_push(FrameQueue *f)
{
	//�����ڵ��ʼѭ����
	if (++f->windex == f->max_size)
		f->windex = 0;

	SDL_LockMutex(f->mutex);
	//size��һ,��������һ���ڵ�
	f->size++;
	//�����пɶ��Ľڵ���
	SDL_CondSignal(f->cond);
	SDL_UnlockMutex(f->mutex);
}

//������ǰӦ�õ��ô˺���������һ�����Զ��Ľڵ�
Frame *frame_queue_peek_readable(FrameQueue *f)
{
	SDL_LockMutex(f->mutex);

	while (f->size <= 0 )
	{
		SDL_CondWait(f->cond, f->mutex);
	}
	SDL_UnlockMutex(f->mutex);

	return &f->queue[f->rindex];
}

//��������Ӧ�õ��ô˺����ѽڵ�rindex���ƫ��һ��
void frame_queue_next(FrameQueue *f)
{
	av_frame_unref(f->queue[f->rindex].frame);

	if (++f->rindex == f->max_size)
	f->rindex = 0;

	SDL_LockMutex(f->mutex);
	//size��һ,���м���һ���ڵ�
	f->size--;
	//�����п�д�Ľڵ���
	SDL_CondSignal(f->cond);
	SDL_UnlockMutex(f->mutex);
}

//��frameֻ������һ�����
void frame_queue_put_nullframe(Frame *f)
{
	f->frame_quit = 1;
}

//frame����
void frame_queue_destory(FrameQueue *f)
{
	int i;
	for (i = 0; i < f->max_size; i++)
	{
		Frame *vp = &f->queue[i];
		av_frame_unref(vp->frame);
		av_frame_free(&vp->frame);
	}
	SDL_DestroyMutex(f->mutex);
	SDL_DestroyCond(f->cond);
}

//--------------------------------------------------------------------

struct SwsContext *video_sws_ctx=NULL;
AVInputFormat *video_iformat=NULL;
const char *video_filename=NULL;
AVFormatContext *video_ic=NULL;
AVStream *video_stream=NULL;
AVCodecContext *video_avctx=NULL;
AVCodec *video_codec=NULL;
FrameQueue f_video_q;
PacketQueue p_video_q;
int video_index = -1;
int default_width=0;
int default_height=0;

SDL_mutex *end_mutex;
SDL_cond *end_cond;
int sdl_quit = 0;

SDL_Window *window;
SDL_Renderer *renderer;
SDL_Texture* texture;
SDL_RendererInfo renderer_info = {0};

//��Ƶˢ���߳�
int video_refresh(void *arg)
{
	int ret = 0;
	
	ret = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);
	if(ret)
	{
		printf("SDL_Init error!\n");
		return 0;
	}
	
	window = SDL_CreateWindow("player", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, default_width, default_height, SDL_WINDOW_RESIZABLE);
	if (window)
	{
		renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
		if (!renderer)
		{
			printf("Failed to initialize a hardware accelerated renderer!\n");
			renderer = SDL_CreateRenderer(window, -1, 0);
		}
		if (renderer)
		{
			if (!SDL_GetRendererInfo(renderer, &renderer_info))
			printf("Initialized %s renderer.\n", renderer_info.name);
		}

		//�����ݺ��
		SDL_RenderSetLogicalSize(renderer, default_width, default_height);

		//Ĭ�����ó�YUV420P��ʽ����,��ʽ��һ����Ҫת��ʽ
		texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, default_width, default_height);

		if (!window || !renderer || !texture || !renderer_info.num_texture_formats)
		{
			printf("Failed to create window or renderer: %s", SDL_GetError());
			return 0;
		}
	}
	else
	{
		return 0;
	}	

	//2.ֻ�����һ��ת��ʽ���frame�����ݴ洢�ռ�,Ĭ�����е�frame����һ���ĸ�ʽ�Ϳ��
	AVFrame *frame_yuv420p = av_frame_alloc();
	int buf_size = av_image_get_buffer_size(AV_PIX_FMT_YUV420P,default_width,default_height,1);
	uint8_t* buffer = (uint8_t *)av_malloc(buf_size);
	av_image_fill_arrays(frame_yuv420p->data, frame_yuv420p->linesize, buffer, AV_PIX_FMT_YUV420P, default_width, default_height, 1);

	//Ĭ�����е�frame����һ���ĸ�ʽ�Ϳ��
	video_sws_ctx = sws_getContext(video_avctx->width, video_avctx->height, video_avctx->pix_fmt,default_width,default_height,AV_PIX_FMT_YUV420P,SWS_BICUBIC,NULL, NULL,NULL);
	if(video_sws_ctx == NULL)
	{
		printf("sws_getContext init error!\n");
		goto fail;
	}

	Frame *af;
	SDL_Event event;
	double time=0.0;
	int pause = 1;
	while(1)
	{
		SDL_PumpEvents();
		while(!SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT) && pause)
		{
			//1.ȡһ��frame
			af = frame_queue_peek_readable(&f_video_q);

			//2.�ж��Ƿ������һ��frame,֪ͨ���߳��˳�
			if(af->frame_quit)
			{
				SDL_LockMutex(end_mutex);
				SDL_CondSignal(end_cond);
				SDL_UnlockMutex(end_mutex);
				break;
			}

			//ת��ʽ
			sws_scale(video_sws_ctx,(const uint8_t *const *)af->frame->data, af->frame->linesize, 0, default_height, frame_yuv420p->data, frame_yuv420p->linesize);

			//����
			SDL_UpdateYUVTexture(texture,NULL,frame_yuv420p->data[0],frame_yuv420p->linesize[0],
											  frame_yuv420p->data[1],frame_yuv420p->linesize[1],
											  frame_yuv420p->data[2],frame_yuv420p->linesize[2]);
			SDL_RenderClear(renderer);
			SDL_RenderCopy(renderer,texture,NULL,NULL);
			SDL_RenderPresent(renderer);
			
			//frame����ƫ��
			frame_queue_next(&f_video_q);
			
			//��ƽ��֡����ʱ
			av_usleep(1000*1000*(double)video_avctx->framerate.den/video_avctx->framerate.num);
			time+= (double)video_avctx->framerate.den/video_avctx->framerate.num;
			printf("����ʱ��:%.3lfs\r", time);			

			SDL_PumpEvents();
			
			//������һ֡
			if(pause == 2)
			pause = 0;
		}

		//�û��¼�����
		switch (event.type)
		{
			case SDL_KEYDOWN:
			switch(event.key.keysym.sym)
			{
				case SDLK_SPACE:
					pause = !pause;
					break;
				case SDLK_s:
					pause = 2;
					break;
			}
			break;
			case SDL_QUIT:
			sdl_quit = 1;
			goto fail;
			default:
			break;
		}		
	}
fail:
	av_frame_free(&frame_yuv420p);
	av_free(buffer);
	
	return 0;
}

//�����߳�
int video_decode(void *arg)
{
	int ret = 0;
	AVFrame *frame = av_frame_alloc();
	AVPacket pkt;
	Frame *af;

	while(1)
	{
		//�û��˳�SDL
		if(sdl_quit)
		{
			av_frame_free(&frame);
			return 0;
		}
		
		ret = avcodec_receive_frame(video_avctx, frame);

		if(ret==0)
		{
			af = frame_queue_peek_writable(&f_video_q);
			av_frame_move_ref(af->frame, frame);
			frame_queue_push(&f_video_q);
		}
		else if (ret == AVERROR(EAGAIN))
		{
			packet_queue_get(&p_video_q, &pkt);
			avcodec_send_packet(video_avctx, &pkt);
			av_packet_unref(&pkt);
		}
		else if (ret == AVERROR_EOF)
		{
			af = frame_queue_peek_writable(&f_video_q);
			frame_queue_put_nullframe(af);
			av_frame_move_ref(af->frame, frame);
			frame_queue_push(&f_video_q);
			break;
		}
		else
		{
			af = frame_queue_peek_writable(&f_video_q);
			frame_queue_put_nullframe(af);
			av_frame_move_ref(af->frame, frame);
			frame_queue_push(&f_video_q);
			break;
		}
	}
	av_frame_free(&frame);

	return 0;
}

//���װ�߳�
int main(int argc, char **argv)
{
	AVPacket pkt1, *pkt = &pkt1;
	int ret=0;
	
	avdevice_register_all();
	avformat_network_init();

	video_filename = av_strdup(argv[1]);
	video_iformat = NULL;
	// video_filename = av_strdup("video=USB2.0 PC CAMERA");
	// video_iformat = av_find_input_format("dshow");

	//���г�ʼ��
	frame_queue_init(&f_video_q, MAX_FRAME_QUEUE_SIZE);
	packet_queue_init(&p_video_q);

	//���ļ�
	ret = avformat_open_input(&video_ic, video_filename, video_iformat, NULL);
	if(ret<0)
	{
		printf("avformat_open_input\n");
		goto fail;
	}

	//������
	ret = avformat_find_stream_info(video_ic, NULL);
	if(ret<0)
	{
		printf("avformat_find_stream_info\n");
		goto fail;
	}

	av_dump_format(video_ic, 0,video_filename, 0);
	printf("--------------------------------------------------------\n");

	//������index
	int i=0;
	for (i = 0; i < video_ic->nb_streams; i++)
	{
		if(video_ic->streams[i]->codecpar->codec_type==AVMEDIA_TYPE_VIDEO)
		{
			video_index=i;
			break;
		}
	}

	if(video_index==-1)
	{
		printf("no video stream!\n");
		goto fail;
	}

	//����ֵ
	video_stream = video_ic->streams[video_index];

	//���������ctx
	video_avctx = avcodec_alloc_context3(NULL);
	if(!video_avctx)
	{
		printf("avcodec_alloc_context3\n");
		goto fail;
	}

	//������������--->������ctx
	avcodec_parameters_to_context(video_avctx, video_stream->codecpar);

	//���ҽ�����
	video_codec = avcodec_find_decoder(video_avctx->codec_id);
	if(!video_codec)
	{
		printf("avcodec_find_decoder\n");
		goto fail;
	}

	//��Ƶ֡��
	video_avctx->framerate = av_guess_frame_rate(video_ic, video_stream, NULL);

	//�򿪽�����
	ret = avcodec_open2(video_avctx, video_codec, NULL);
	if(ret<0)
	{
		printf("avformat_open_input\n");
		goto fail;
	}

	default_width=video_avctx->width;
	default_height=video_avctx->height;

	//���������߳�
	SDL_Thread *thread1 = SDL_CreateThread(video_decode, "video_decode", NULL);
	if(!thread1)
	{
		printf("SDL_CreateThread\n");
		goto fail;
	}

	//������Ƶ�����߳�
	SDL_Thread *thread2 = SDL_CreateThread(video_refresh, "video_refresh", NULL);
	if(!thread2)
	{
		printf("SDL_CreateThread\n");
		goto fail;
	}

	//���ﴴ��һ����������,�������̵߳ȴ������߳���ɺ�,���˳�
	end_mutex = SDL_CreateMutex();
	end_cond = SDL_CreateCond();

	//���߳�
	while(1)
	{
		//�û��˳�SDL
		if(sdl_quit)
		goto fail;
		
		if(packet_queue_num(&p_video_q) > 500)
		{
			av_usleep(10000);
			continue;
		}

		ret = av_read_frame(video_ic, pkt);
		if (ret < 0)
		{
			if (ret == AVERROR_EOF || avio_feof(video_ic->pb))
			{
				packet_queue_put_nullpacket(&p_video_q, video_index);
				SDL_LockMutex(end_mutex);
				SDL_CondWait(end_cond, end_mutex);
				SDL_UnlockMutex(end_mutex);
				break;
			}
			else
			{
				packet_queue_put_nullpacket(&p_video_q, video_index);
				SDL_LockMutex(end_mutex);
				SDL_CondWait(end_cond, end_mutex);
				SDL_UnlockMutex(end_mutex);
				break;
			}
		}

		//��Ƶ�������,����������
		if (pkt->stream_index ==video_index)
		{
			av_packet_rescale_ts(pkt, video_stream->time_base, video_avctx->time_base);
			packet_queue_put(&p_video_q, pkt);
		}
		else
		{
			av_packet_unref(pkt);
		}
	}

fail:
	packet_queue_destroy(&p_video_q);
	frame_queue_destory(&f_video_q);
	avformat_free_context(video_ic);
	avcodec_free_context(&video_avctx);
	
	return 0;
}











































