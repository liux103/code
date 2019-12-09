

/*				输入流转解码器时间基							   转成编码器时间基								转成输出流时间基
读取一个packet------------------------>送入解码器--->输出frame--->------------------->送入编码器--->输出packet-------------------->写文件
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

	//打开文件
    if ((ret = avformat_open_input(&ifmt_ctx, filename, NULL, NULL)) < 0)
	{
        printf("Cannot open input file\n");
        return ret;
    }

	//读取流
    if ((ret = avformat_find_stream_info(ifmt_ctx, NULL)) < 0) 
	{
        printf("Cannot find stream information\n");
        return ret;
    }

	//查找流
    for (i = 0; i < ifmt_ctx->nb_streams; i++)
	{
		AVStream *stream = ifmt_ctx->streams[i];
		
		//这里只处理视频流
		if(stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			//查找流对应的解码器
			AVCodec *dec = avcodec_find_decoder(stream->codecpar->codec_id);
			if (!dec) 
			{
				printf("Failed to find decoder for stream #%u\n", i);
				return AVERROR_DECODER_NOT_FOUND;
			}			
			
			//根据解码器分配解码器ctx
			AVCodecContext *codec_ctx = avcodec_alloc_context3(dec);
			if (!codec_ctx)
			{
				printf("Failed to allocate the decoder context for stream #%u\n", i);
				return AVERROR(ENOMEM);
			}
			
			//copy解码器数据到解码器ctx
			ret = avcodec_parameters_to_context(codec_ctx, stream->codecpar);
			if (ret < 0)
			{
				printf("Failed to copy decoder parameters to input decoder context for stream #%u\n", i);
				return ret;
			}
			
			//视频流获取一下帧率,如果需要的话
			codec_ctx->framerate = av_guess_frame_rate(ifmt_ctx, stream, NULL);
			
			//打开解码器
            ret = avcodec_open2(codec_ctx, dec, NULL);
            if (ret < 0) 
			{
                printf("Failed to open decoder for stream #%u\n", i);
                return ret;
            }
			
			//保存解码器ctx  bit_rate
			dec_ctx = codec_ctx;
			
			fprintf(log_fp, "1.输入ctx时间基:%ld/%ld\n", AV_TIME_BASE_Q.num, AV_TIME_BASE_Q.den);
			fprintf(log_fp, "2.输入ctx起始时间:%ld\n", ifmt_ctx->start_time);
			fprintf(log_fp, "3.输入ctx持续时间:%ld\n", ifmt_ctx->duration);
			fprintf(log_fp, "4.输入ctx总码率:%ld\n", ifmt_ctx->bit_rate);
			fprintf(log_fp, "5.输入视频流时间基:%ld/%ld\n", stream->time_base.num, stream->time_base.den);
			fprintf(log_fp, "6.输入视频流持续时间:%ld\n", stream->duration);
			fprintf(log_fp, "7.解码器时间基:%ld/%ld\n", dec_ctx->time_base.num, dec_ctx->time_base.den);
			fprintf(log_fp, "8.解码器码率 :%ld\n", dec_ctx->bit_rate);
			fprintf(log_fp, "9.视频帧率:%d\n", dec_ctx->framerate);
			fprintf(log_fp, "10.像素格式:%d\n", dec_ctx->pix_fmt);
		}
	}
	
    av_dump_format(ifmt_ctx, 0, filename, 0);
    return 0;
}

static int open_output_file(const char *filename)
{
    int ret;
    unsigned int i;

	//打开输出文件
    avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, filename);
    if (!ofmt_ctx) 
	{
        printf("Could not create output context\n");
        return AVERROR_UNKNOWN;
    }

	//输出流操作
    for (i = 0; i < ifmt_ctx->nb_streams; i++)
	{
		//输入流
		AVStream *stream = ifmt_ctx->streams[i];
		
		if(stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
		{	
			//为ofmt_ctx分配一个视频输出输出流
			AVStream *out_stream = avformat_new_stream(ofmt_ctx, NULL);
			if (!out_stream) 
			{
				printf("Failed allocating output stream\n");
				return AVERROR_UNKNOWN;
			}
			
			//这里编码器设置成和解码器一样,也可以指定编码器,AV_CODEC_ID_H264
			AVCodec *encoder = avcodec_find_encoder(AV_CODEC_ID_H264);
			if (!encoder) 
			{
				printf("Necessary encoder not found\n");
				return AVERROR_INVALIDDATA;
			}
			//分配编码器ctx
			AVCodecContext *codec_ctx = avcodec_alloc_context3(encoder);
			if (!codec_ctx) 
			{
				printf("Failed to allocate the encoder context\n");
				return AVERROR(ENOMEM);
			}

			//这里编码器一些参数设置成和输入文件一样
			codec_ctx->height = dec_ctx->height;
			codec_ctx->width = dec_ctx->width;
	
			//PAR —— Pixel Aspect Ratio 像素横纵比。表示每个像素的宽度与长度的比值。可以认为每个像素不是正方形的。
			//DAR —— Display Aspect Ratio 显示横纵比。最终显示的图像在长度单位上的横纵比。
			//SAR —— Sample Aspect Ratio 采样横纵比。表示横向的像素点数和纵向的像素点数的比值。
			codec_ctx->sample_aspect_ratio = dec_ctx->sample_aspect_ratio;
	
			//编码的像素格式,如果与解码出来的格式不同,则需要转格式
			codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
		
			//根据解码器的帧率算出编码器的时间基,这里编解码器时间基一样,可以少一次时间基转换
			//codec_ctx->time_base = av_inv_q(dec_ctx->framerate);
			codec_ctx->time_base = dec_ctx->time_base;
			
			//码率设置一样(实际会取整),降低码率会降低视频质量,损失的是哪一部分数据？？？
			codec_ctx->bit_rate = dec_ctx->bit_rate;

			//不编码B帧,便于处理pts和dts
			codec_ctx->max_b_frames = 0;
			
			//编解码器帧率一样
			codec_ctx->framerate = dec_ctx->framerate;
			
			//文件预览标志？？？暂时没研究
			if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
				codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

			//打开编码器,里面是做的一些初始化的工作
			ret = avcodec_open2(codec_ctx, encoder, NULL);
			if (ret < 0)
			{
				printf("Cannot open video encoder for stream #%u\n", i);
				return ret;
			}
			
			//输入输出流时间基设置成一样
			out_stream->time_base = stream->time_base;
			
			//输入输出流持续时间基设置成一样
			out_stream->duration = stream->duration;
			
			//初始化编码器
			ret = avcodec_parameters_from_context(out_stream->codecpar, codec_ctx);
			if (ret < 0) 
			{
				printf("Failed to copy encoder parameters to output stream #%u\n", i);
				return ret;
			}
			
			//保存编码器ctx
			enc_ctx = codec_ctx;
			
			fprintf(log_fp, "1.输出ctx时间基:%ld/%ld\n", AV_TIME_BASE_Q.num, AV_TIME_BASE_Q.den);
			fprintf(log_fp, "2.输出ctx起始时间:%ld\n", ofmt_ctx->start_time);
			fprintf(log_fp, "3.输出ctx持续时间:%ld\n", ofmt_ctx->duration);
			fprintf(log_fp, "4.输出ctx总码率:%ld\n", ofmt_ctx->bit_rate);
			fprintf(log_fp, "5.输出视频流时间基:%ld/%ld\n", out_stream->time_base.num, out_stream->time_base.den);
			fprintf(log_fp, "6.输出视频流持续时间:%ld\n", out_stream->duration);
			fprintf(log_fp, "7.编码器时间基:%ld/%ld\n", enc_ctx->time_base.num, enc_ctx->time_base.den);
			fprintf(log_fp, "8.编码器码率 :%ld\n", enc_ctx->bit_rate);
			fprintf(log_fp, "9.视频帧率:%d\n", enc_ctx->framerate);
			fprintf(log_fp, "10.像素格式:%d\n", enc_ctx->pix_fmt);		
		}
	}
    av_dump_format(ofmt_ctx, 0, filename, 1);

	//打开输出文件
    if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) 
	{
        ret = avio_open(&ofmt_ctx->pb, filename, AVIO_FLAG_WRITE);
        if (ret < 0) 
		{
            printf("Could not open output file '%s'", filename);
            return ret;
        }
    }

    //写文件头
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
    
	//开始编码
    ret = avcodec_encode_video2(enc_ctx, &enc_pkt, sws_frame, got_frame);
    av_frame_free(&sws_frame);
    if (ret < 0)
        return ret;
    if (!(*got_frame))
        return 0;
	
	//printf("index2:%d\n", ++index2);
	
	fprintf(log_fp, "编码时间基:time_base:%d/%ld\n", enc_ctx->time_base.num, enc_ctx->time_base.den);
	fprintf(log_fp, "编码后的包:pts=%lld\n", enc_pkt.pts);
	fprintf(log_fp, "编码后的包:dts=%lld\n", enc_pkt.dts);
	fprintf(log_fp, "编码后的包:duration=%lld\n", enc_pkt.duration);
	fprintf(log_fp, "----------------------------------\n");
	
    //设置流对应的index
    enc_pkt.stream_index = stream_index;
	
	//时间基转换enc_ctx.time_base---->ofmt_ctx.time_base
    av_packet_rescale_ts(&enc_pkt, enc_ctx->time_base, ofmt_ctx->streams[stream_index]->time_base);
	fprintf(log_fp, "输出流时间基:time_base:%d/%ld\n", ofmt_ctx->streams[stream_index]->time_base.num, ofmt_ctx->streams[stream_index]->time_base.den);
	fprintf(log_fp, "转输出流后的包:pts=%lld\n", enc_pkt.pts);
	fprintf(log_fp, "转输出流后的包:dts=%lld\n", enc_pkt.dts);
	fprintf(log_fp, "转输出流后的包:duration=%lld\n", enc_pkt.duration);
	fprintf(log_fp, "--------------end--------------------\n");	

    printf("Muxing frame\n");
	
    //写文件,开始封装
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
			//判断读取到的packet的pts与dts时间戳是不是一样的,调试用
			if(packet.pts != packet.dts)
			{
				fprintf(log_fp, "packet.pts != packet.dts\n");
			}
		
			//使用AVStream里面的time_base=1/1000 0000
			fprintf(log_fp, "----------------start------------------\n");
			fprintf(log_fp, "输入流时间基:%d/%d\n", ifmt_ctx->streams[stream_index]->time_base.num,ifmt_ctx->streams[stream_index]->time_base.den);
			fprintf(log_fp, "读取的包:pts=%lld\n", packet.pts);
			fprintf(log_fp, "读取的包:dts=%lld\n", packet.dts);
			fprintf(log_fp, "读取的包:duration=%lld\n", packet.duration);
			fprintf(log_fp, "----------------------------------\n");
			
			//把ifmt_ctx流里面的时间基转成codec流里面的时间基
            av_packet_rescale_ts(&packet, ifmt_ctx->streams[stream_index]->time_base, dec_ctx->time_base);
			
			fprintf(log_fp, "解码器时间基:%d/%d\n", dec_ctx->time_base.num, dec_ctx->time_base.den);
			fprintf(log_fp, "转换包:pts=%lld\n", packet.pts);
			fprintf(log_fp, "转换包:dts=%lld\n", packet.dts);
			fprintf(log_fp, "转换包:duration=%lld\n", packet.duration);
			fprintf(log_fp, "----------------------------------\n");	

			//开始解码
            ret = avcodec_decode_video2(dec_ctx, frame, &got_frame, &packet);
            if (ret < 0)
			{
                printf("Decoding failed\n");
                break;
            }
			//pts pkt_dts pkt_duration ==copy自packet, pkt_duration采用AVStream时间基？？？
			fprintf(log_fp, "解码后时间基:%d/%d\n", dec_ctx->time_base.num, dec_ctx->time_base.den);
			fprintf(log_fp, "解码后帧:pts=%lld\n", frame->pts);
			fprintf(log_fp, "解码后帧:dts=%lld\n", frame->pkt_dts);
			fprintf(log_fp, "解码后帧:duration=%lld\n", frame->pkt_duration);
			fprintf(log_fp, "解码后帧:best_effort_timestamp=%d\n", frame->best_effort_timestamp);
			fprintf(log_fp, "解码后帧:format=%d\n", frame->format);
			fprintf(log_fp, "解码后帧:width=%d\n", frame->width);
			fprintf(log_fp, "解码后帧:height=%d\n", frame->height);
			fprintf(log_fp, "I P B:type=%d\n", frame->pict_type);
			fprintf(log_fp, "----------------------------------\n");	

			//查看解码有没有B帧,调试用
			if(frame->pict_type == AV_PICTURE_TYPE_B || frame->pict_type == AV_PICTURE_TYPE_BI)
			{
				fprintf(log_fp, "bas B frame!!!\n");
			}
			
			//解码成功
            if(got_frame)
			{
				//sdl_show(frame);
				//printf("index1:%d\n", ++index1);
				
				//重新写一下frame pts,然后开始编码
                frame->pts = frame->best_effort_timestamp;
				ret = encode_write_frame(frame, stream_index, &got_frame);
				
                if (ret < 0)
				goto end;
            }   
        } 
		av_frame_free(&frame); 
        av_packet_unref(&packet);
    }

    //冲洗编码器
    ret = flush_encoder(stream_index);
    if (ret < 0)
	{
        printf("Flushing encoder failed\n");
        goto end;
    }
   
	//写输出文件
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

















