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

typedef struct MyAVPacketList 
{
    AVPacket pkt;
    struct MyAVPacketList *next;
} MyAVPacketList;

typedef struct PacketQueue 
{	
    MyAVPacketList *first_pkt; 	//队列头
	MyAVPacketList *last_pkt;	//队列尾
    int nb_packets;				//队列中包的个数						
    int size;					//队列中包的size(byte)							
    SDL_mutex *mutex;			//锁		
    SDL_cond *cond;				//条件变量		
} PacketQueue;

int packet_queue_init(PacketQueue *q)
{
    memset(q, 0, sizeof(PacketQueue));
    q->mutex = SDL_CreateMutex();
    q->cond = SDL_CreateCond();
	
    return 0;
}

int packet_queue_put(PacketQueue *q, AVPacket *pkt)
{
    int ret;
	MyAVPacketList *pkt1;

	//构造一个节点
	pkt1=av_malloc(sizeof(MyAVPacketList));
    pkt1->pkt = *pkt;
    pkt1->next = NULL;

    SDL_LockMutex(q->mutex);

    if(!q->last_pkt)//末尾节点为空
        q->first_pkt = pkt1;
    else
       q->last_pkt->next = pkt1;
   
	q->last_pkt = pkt1;
    
    q->nb_packets++;
    q->size += pkt1->pkt.size + sizeof(*pkt1);

    SDL_CondSignal(q->cond);

    SDL_UnlockMutex(q->mutex);

    return ret;
}

int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block)
{
    MyAVPacketList *pkt1;
    int ret;

    SDL_LockMutex(q->mutex);

    for (;;) 
	{
        pkt1 = q->first_pkt;
		
        if (pkt1)//取包成功
		{
            q->first_pkt = pkt1->next;
            if (!q->first_pkt)
                q->last_pkt = NULL;
			
            q->nb_packets--;
            q->size -= pkt1->pkt.size + sizeof(*pkt1);
			
            *pkt = pkt1->pkt;
			
            av_free(pkt1);
			
            ret = 1;
            break;
        } 
		else if (!block)//没有包
		{
            ret = 0;
            break;
        } 
		else//队列空,等待新包
		{
            SDL_CondWait(q->cond, q->mutex);
        }
    }
    SDL_UnlockMutex(q->mutex);
    return ret;
}

static void packet_queue_destroy(PacketQueue *q)
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

int main(int argc, char **argv)
{
	
	AVPacket flush_pkt1;
	AVPacket flush_pkt2;
	AVPacket flush_pkt3;
	av_init_packet(&flush_pkt1);
	av_init_packet(&flush_pkt2);
	av_init_packet(&flush_pkt3);
	// printf("packet1:%p\n", &flush_pkt1);
	// printf("packet2:%p\n", &flush_pkt2);
	// printf("packet3:%p\n", &flush_pkt3);
	
	AVPacket pkt;
	
	PacketQueue *q_audio;
	
	packet_queue_init(q_audio);
	
	

	
	packet_queue_put(q_audio, &flush_pkt1);
	printf("first_pkt:%p\n", q_audio->first_pkt);
	printf("first_pkt.next:%p\n", q_audio->first_pkt->next);
	printf("last_pkt:%p\n",  q_audio->last_pkt);
	printf("last_pkt.next:%p\n",  q_audio->last_pkt->next);
	
	system("pause");
	
	packet_queue_put(q_audio, &flush_pkt2);
	printf("first_pkt:%p\n", q_audio->first_pkt);
	printf("first_pkt.next:%p\n", q_audio->first_pkt->next);
	printf("last_pkt:%p\n",  q_audio->last_pkt);
	printf("last_pkt.next:%p\n",  q_audio->last_pkt->next);	
	
	system("pause");
	
	packet_queue_put(q_audio, &flush_pkt3);
	printf("first_pkt:%p\n", q_audio->first_pkt);
	printf("first_pkt.next:%p\n", q_audio->first_pkt->next);
	printf("last_pkt:%p\n",  q_audio->last_pkt);
	printf("last_pkt.next:%p\n",  q_audio->last_pkt->next);		
	
	system("pause");

	packet_queue_get(q_audio, &pkt, 0);
	printf("first_pkt:%p\n", q_audio->first_pkt);
	printf("first_pkt.next:%p\n", q_audio->first_pkt->next);
	printf("last_pkt:%p\n",  q_audio->last_pkt);
	printf("last_pkt.next:%p\n",  q_audio->last_pkt->next);			
	
	
	while(1)
	{

	}
	
	return 0;
}











































