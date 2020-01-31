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

//packet缓冲队列节点
typedef struct MyAVPacketList
{
	AVPacket pkt;
	struct MyAVPacketList *next;
} MyAVPacketList;

//packet队列结构体里面只保存了首节点和尾节点,中间节点
//通过MyAVPacketList->next连接起来,packet入队列时就插
//入到队列尾部,packet出队列时就从首部取packet
typedef struct PacketQueue
{
	MyAVPacketList *first_pkt;	//首节点
	MyAVPacketList *last_pkt;	//尾节点
	int abort_request;			//中断请求
	int nb_packets;				//队列总共packet个数
	int size;					//所有packet的size大小
	SDL_mutex *mutex;			//锁
	SDL_cond *cond;				//条件变量
} PacketQueue;

//packet队列初始化
int packet_queue_init(PacketQueue *q)
{
	//初始化结构体成员
	memset(q, 0, sizeof(PacketQueue));

	//因为packet队列会被多个线程访问
	//所以这里需要创建锁来保护资源
	q->mutex = SDL_CreateMutex();
	if(!q->mutex)
	{
		return -1;
	}
	//条件变量
	q->cond = SDL_CreateCond();
	if(!q->mutex)
	{
		return -1;
	}

	return 0;
}

//packet入队列
int packet_queue_put(PacketQueue *q, AVPacket *pkt)
{
	SDL_LockMutex(q->mutex);

	//1.先构造一个节点,这个节点需要插入到队列尾部
	MyAVPacketList *pkt1;
	pkt1=av_malloc(sizeof(MyAVPacketList));
	if(!pkt1)
	{
		return -1;
	}
	pkt1->pkt = *pkt;
	pkt1->next = NULL;

	//2.这里的逻辑需要处理的就是把新的节点挂载到当前
	//尾节点的next上去,起到连接作用.然后步骤3会把
	//PacketQueue的尾节点更新为本次构造的节点
	if(!q->last_pkt)
		q->first_pkt = pkt1;
	else
		q->last_pkt->next = pkt1;

	//3.更新PacketQueue的尾节点为刚刚构造的节点
	q->last_pkt = pkt1;

	//4.packet个数加一
	q->nb_packets++;
	
	//5.packet大小增加(可以用作内存占用统计)
	q->size += pkt1->pkt.size + sizeof(*pkt1);

	//6.有一个packet入队列了,这里通知那些等待
	//packet的线程可以从队列取packet了
	SDL_CondSignal(q->cond);

	SDL_UnlockMutex(q->mutex);

	return 0;
}

//packet空包入队列
int packet_queue_put_nullpacket(PacketQueue *q, int stream_index)
{
	//构造空包是为了文件结束时,往解码器发送空包,用来冲洗解码器
	AVPacket pkt1, *pkt = &pkt1;
	av_init_packet(pkt);
	pkt->data = NULL;
	pkt->size = 0;
	pkt->stream_index = stream_index;
	return packet_queue_put(q, pkt);
}

//packet出队列
int packet_queue_get(PacketQueue *q, AVPacket *pkt)
{
	MyAVPacketList *pkt1;
	int ret;

	SDL_LockMutex(q->mutex);	
	for (;;)
	{
		//1.取首节点	
		pkt1 = q->first_pkt;

		//队列有packet
		if (pkt1)
		{
			//2.把队列首节点更新一下
			q->first_pkt = pkt1->next;
			if (!q->first_pkt)
				q->last_pkt = NULL;

			//3.包个数减一
			q->nb_packets--;
			//4.packet大小减掉
			q->size -= pkt1->pkt.size + sizeof(*pkt1);
			//5.取packet
			*pkt = pkt1->pkt;
			//6.释放资源
			av_free(pkt1);
			ret = 1;
			break;
		}
		else
		{
			//如果队列没有包,这里会一直阻塞
			SDL_CondWait(q->cond, q->mutex);
		}
	}
	SDL_UnlockMutex(q->mutex);
	return ret;
}

//packet个数,可以用来控制队列长度
//防止无限缓冲把内存耗尽
int packet_queue_num(PacketQueue *q)
{
	return q->nb_packets;
}

//packet销毁
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

//frame节点
typedef struct Frame
{
	AVFrame *frame;	//frame
	int frame_quit;	//标记该frame是否是最后一个frame
} Frame;

typedef struct FrameQueue
{
	Frame queue[MAX_FRAME_QUEUE_SIZE];//用数组来表示节点
	int rindex;			//标记当前可读的Frame的index
	int windex;			//标记当前可写的Frame的index
	int size;			//标记当前队列已经写了多少个
	int max_size;		//队列最大节点数
	SDL_mutex *mutex;	//锁
	SDL_cond *cond;		//条件变量
} FrameQueue;

//frame初始化
int frame_queue_init(FrameQueue *f, int max_size)
{
	int i;
	//初始化成员
	memset(f, 0, sizeof(FrameQueue));

	//分配锁
	f->mutex = SDL_CreateMutex();
	if(!f->mutex)
	{
		return -1;
	}
	//条件变量
	f->cond = SDL_CreateCond();
	if(!f->cond)
	{
		return -1;
	}
	//最大节点数
	f->max_size = max_size;
	
	//分配好所有的节点
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

//写操作前应该调用此函数来查找一个可以写的节点
Frame *frame_queue_peek_writable(FrameQueue *f)
{
	SDL_LockMutex(f->mutex);
	
	//如果都当前已经写入队列的个数等于最大节点数
	//说明当前队列已经满了,没有位置写了,这种情况
	//下就等待,直到队列空出位置来
	while(f->size >= f->max_size)
	{
		SDL_CondWait(f->cond, f->mutex);
	}
	SDL_UnlockMutex(f->mutex);

	//这里只返回一个可写的frame节点,并没有写数据
	return &f->queue[f->windex];
}

//写操作后应该调用此函数把节点windex向后偏移一下
void frame_queue_push(FrameQueue *f)
{
	//到最大节点后开始循环了
	if (++f->windex == f->max_size)
		f->windex = 0;

	SDL_LockMutex(f->mutex);
	//size加一,队列新增一个节点
	f->size++;
	//队列有可读的节点了
	SDL_CondSignal(f->cond);
	SDL_UnlockMutex(f->mutex);
}

//读操作前应该调用此函数来查找一个可以读的节点
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

//读操作后应该调用此函数把节点rindex向后偏移一下
void frame_queue_next(FrameQueue *f)
{
	av_frame_unref(f->queue[f->rindex].frame);

	if (++f->rindex == f->max_size)
	f->rindex = 0;

	SDL_LockMutex(f->mutex);
	//size减一,队列减少一个节点
	f->size--;
	//队列有可写的节点了
	SDL_CondSignal(f->cond);
	SDL_UnlockMutex(f->mutex);
}

//空frame只是做了一个标记
void frame_queue_put_nullframe(Frame *f)
{
	f->frame_quit = 1;
}

//frame销毁
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

//视频刷新线程
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

		//保持纵横比
		SDL_RenderSetLogicalSize(renderer, default_width, default_height);

		//默认设置成YUV420P格式播放,格式不一样需要转格式
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

	//2.只需分配一次转格式后的frame的数据存储空间,默认所有的frame都是一样的格式和宽高
	AVFrame *frame_yuv420p = av_frame_alloc();
	int buf_size = av_image_get_buffer_size(AV_PIX_FMT_YUV420P,default_width,default_height,1);
	uint8_t* buffer = (uint8_t *)av_malloc(buf_size);
	av_image_fill_arrays(frame_yuv420p->data, frame_yuv420p->linesize, buffer, AV_PIX_FMT_YUV420P, default_width, default_height, 1);

	//默认所有的frame都是一样的格式和宽高
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
			//1.取一个frame
			af = frame_queue_peek_readable(&f_video_q);

			//2.判断是否是最后一个frame,通知主线程退出
			if(af->frame_quit)
			{
				SDL_LockMutex(end_mutex);
				SDL_CondSignal(end_cond);
				SDL_UnlockMutex(end_mutex);
				break;
			}

			//转格式
			sws_scale(video_sws_ctx,(const uint8_t *const *)af->frame->data, af->frame->linesize, 0, default_height, frame_yuv420p->data, frame_yuv420p->linesize);

			//播放
			SDL_UpdateYUVTexture(texture,NULL,frame_yuv420p->data[0],frame_yuv420p->linesize[0],
											  frame_yuv420p->data[1],frame_yuv420p->linesize[1],
											  frame_yuv420p->data[2],frame_yuv420p->linesize[2]);
			SDL_RenderClear(renderer);
			SDL_RenderCopy(renderer,texture,NULL,NULL);
			SDL_RenderPresent(renderer);
			
			//frame缓冲偏移
			frame_queue_next(&f_video_q);
			
			//按平均帧率延时
			av_usleep(1000*1000*(double)video_avctx->framerate.den/video_avctx->framerate.num);
			time+= (double)video_avctx->framerate.den/video_avctx->framerate.num;
			printf("播放时间:%.3lfs\r", time);			

			SDL_PumpEvents();
			
			//播放下一帧
			if(pause == 2)
			pause = 0;
		}

		//用户事件处理
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

//解码线程
int video_decode(void *arg)
{
	int ret = 0;
	AVFrame *frame = av_frame_alloc();
	AVPacket pkt;
	Frame *af;

	while(1)
	{
		//用户退出SDL
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

//解封装线程
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

	//队列初始化
	frame_queue_init(&f_video_q, MAX_FRAME_QUEUE_SIZE);
	packet_queue_init(&p_video_q);

	//打开文件
	ret = avformat_open_input(&video_ic, video_filename, video_iformat, NULL);
	if(ret<0)
	{
		printf("avformat_open_input\n");
		goto fail;
	}

	//查找流
	ret = avformat_find_stream_info(video_ic, NULL);
	if(ret<0)
	{
		printf("avformat_find_stream_info\n");
		goto fail;
	}

	av_dump_format(video_ic, 0,video_filename, 0);
	printf("--------------------------------------------------------\n");

	//查找流index
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

	//流赋值
	video_stream = video_ic->streams[video_index];

	//分配解码器ctx
	video_avctx = avcodec_alloc_context3(NULL);
	if(!video_avctx)
	{
		printf("avcodec_alloc_context3\n");
		goto fail;
	}

	//流解码器参数--->解码器ctx
	avcodec_parameters_to_context(video_avctx, video_stream->codecpar);

	//查找解码器
	video_codec = avcodec_find_decoder(video_avctx->codec_id);
	if(!video_codec)
	{
		printf("avcodec_find_decoder\n");
		goto fail;
	}

	//视频帧率
	video_avctx->framerate = av_guess_frame_rate(video_ic, video_stream, NULL);

	//打开解码器
	ret = avcodec_open2(video_avctx, video_codec, NULL);
	if(ret<0)
	{
		printf("avformat_open_input\n");
		goto fail;
	}

	default_width=video_avctx->width;
	default_height=video_avctx->height;

	//创建解码线程
	SDL_Thread *thread1 = SDL_CreateThread(video_decode, "video_decode", NULL);
	if(!thread1)
	{
		printf("SDL_CreateThread\n");
		goto fail;
	}

	//创建视频播放线程
	SDL_Thread *thread2 = SDL_CreateThread(video_refresh, "video_refresh", NULL);
	if(!thread2)
	{
		printf("SDL_CreateThread\n");
		goto fail;
	}

	//这里创建一个条件变量,用于主线程等待播放线程完成后,再退出
	end_mutex = SDL_CreateMutex();
	end_cond = SDL_CreateCond();

	//主线程
	while(1)
	{
		//用户退出SDL
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

		//视频包入队列,其他包丢弃
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











































