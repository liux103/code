#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
#include <windows.h>

AVFormatContext *ifmt_ctx=NULL;
AVFormatContext *ofmt_ctx=NULL;

AVCodecContext *dec_ctx=NULL;
AVCodecContext *enc_ctx=NULL;

FILE *fp_log = NULL;

int open_input_file(const char *filename)
{
    int ret;
    int i=0;
	int video_index = -1;
	AVDictionary *options=NULL;
	
	//设置缓冲,默认大小3041280
	av_dict_set_int(&options, "rtbufsize", 3041280 * 100, 0);

	//打开设备
	AVInputFormat *input_device = av_find_input_format("dshow");
	if(!input_device)
	{
		fprintf(fp_log, "1.av_find_input_format() error!!!\n");
		return ret;
	}	

	//打开摄像头
    if((ret = avformat_open_input(&ifmt_ctx, filename, input_device, &options)) < 0)
	{
        fprintf(fp_log, "2.avformat_open_input() error!!!\n");
        return ret;
    }

	//读取流
    if((ret = avformat_find_stream_info(ifmt_ctx, NULL)) < 0)
	{
        fprintf(fp_log, "3.avformat_find_stream_info() error!!!\n");
        return ret;
    }

	//查找视频流index
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
        fprintf(fp_log, "4.not find video index error!!!\n");
        return ret;	
	}
	
	AVStream *stream = ifmt_ctx->streams[video_index];

	//查找流对应的解码器 AV_CODEC_ID_RAWVIDEO原始压缩格式 13
	AVCodec *dec = avcodec_find_decoder(stream->codecpar->codec_id);
	if(!dec) 
	{
		fprintf(fp_log, "5.avcodec_find_decoder() error!!!\n");
		return -1;
	}			
	
	//根据解码器分配解码器ctx
	AVCodecContext *codec_ctx = avcodec_alloc_context3(dec);
	if (!codec_ctx)
	{
		fprintf(fp_log, "6.avcodec_alloc_context3() error!!!\n");
		return -1;
	}
	
	//copy解码器数据到解码器ctx
	ret = avcodec_parameters_to_context(codec_ctx, stream->codecpar);
	if(ret < 0)
	{
		fprintf(fp_log, "7.avcodec_parameters_to_context() error!!!\n");
		return ret;
	}

	//视频流获取一下帧率,如果需要的话
	codec_ctx->framerate = av_guess_frame_rate(ifmt_ctx, stream, NULL);
	
	//打开解码器
    ret = avcodec_open2(codec_ctx, dec, NULL);
    if(ret < 0) 
	{
        fprintf(fp_log, "8.avcodec_open2() error!!!\n");
        return ret;
    }
	
	//保存解码器ctx
	dec_ctx = codec_ctx;

	//打印时间基帧率相关信息
	fprintf(fp_log, "===========input===========\n");
	fprintf(fp_log, "AVFormatContext:\n");
	fprintf(fp_log, "1.AV_TIME_BASE:%lld\n", AV_TIME_BASE);
	fprintf(fp_log, "2.AV_TIME_BASE_Q:%d/%ld\n", AV_TIME_BASE_Q.num, AV_TIME_BASE_Q.den);
	fprintf(fp_log, "3.start_time:%lld\n", ifmt_ctx->start_time);
	fprintf(fp_log, "4.duration:%ld\n", ifmt_ctx->duration);
	fprintf(fp_log, "5.bit_rate:%ld\n", ifmt_ctx->bit_rate);
	
	fprintf(fp_log, "\nAVStream:\n");
	fprintf(fp_log, "1.time_base:%d/%ld\n", stream->time_base.num, stream->time_base.den);
	fprintf(fp_log, "2.start_time:%lld\n", stream->start_time);
	fprintf(fp_log, "3.duration:%ld\n", stream->duration);

	fprintf(fp_log, "\ndec_ctx:\n");
	fprintf(fp_log, "1.time_base:%d/%ld\n", dec_ctx->time_base.num, dec_ctx->time_base.den);
	fprintf(fp_log, "2.pix_fmt:%d\n", dec_ctx->pix_fmt);
	fprintf(fp_log, "3.decodec_id:%d\n", stream->codecpar->codec_id);
	fprintf(fp_log, "4.width:%d\n", dec_ctx->width);
	fprintf(fp_log, "5.height:%d\n", dec_ctx->height);
	
    //av_dump_format(ifmt_ctx, 0, filename, 0);
	
    return 0;
}

int open_output_file(const char *filename)
{
    int ret;
    unsigned int i;
	int video_index = -1;
	
	//打开输出文件
    avformat_alloc_output_context2(&ofmt_ctx, NULL, "flv", filename);
    if(!ofmt_ctx) 
	{
		fprintf(fp_log, "1.avformat_alloc_output_context2() error!!!\n");
        return -1;
    }

	//输出流操作
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
        fprintf(fp_log, "2.not find video index error!!!\n");
        return -1;
	}
	
	//分配一个输出流
	AVStream *out_stream = avformat_new_stream(ofmt_ctx, NULL);
	if (!out_stream)
	{
		fprintf(fp_log, "3.avformat_new_stream() error!!!\n");
		return -1;
	}
	
	//这里编码器设置成和解码器一样,也可以指定编码器,AV_CODEC_ID_H264
	AVCodec *encoder = avcodec_find_encoder(AV_CODEC_ID_H264);
	if (!encoder) 
	{
		fprintf(fp_log, "4.avcodec_find_encoder() error!!!\n");
		return -1;
	}
	//分配编码器ctx
	AVCodecContext *codec_ctx = avcodec_alloc_context3(encoder);
	if (!codec_ctx) 
	{
		fprintf(fp_log, "5.avcodec_alloc_context3() error!!!\n");
		return -1;
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
	
	//根据解码器的帧率算出编码器的时间基
	codec_ctx->time_base = av_inv_q(dec_ctx->framerate);
	
	//不编码B帧,便于处理pts和dts
	codec_ctx->max_b_frames = 0;	
	
	//文件预览标志？暂时没研究
	if(ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
	codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

	//打开编码器,里面是做的一些初始化的工作
	ret = avcodec_open2(codec_ctx, encoder, NULL);
	if (ret < 0) 
	{
		fprintf(fp_log, "6.avcodec_open2() error!!!\n");
		return -1;
	}
	
	//初始化编码器
	ret = avcodec_parameters_from_context(out_stream->codecpar, codec_ctx);
	if (ret < 0) 
	{
		fprintf(fp_log, "7.avcodec_parameters_from_context() error!!!\n");
		return -1;
	}
	
	//输出流时间基赋值
	out_stream->time_base = codec_ctx->time_base;
	
	//打开输出文件
    if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) 
	{
        ret = avio_open(&ofmt_ctx->pb, filename, AVIO_FLAG_WRITE);
        if (ret < 0) 
		{
            fprintf(fp_log, "8.avio_open() error!!!\n");
            return -1;
        }
    }

    //写文件头
    ret = avformat_write_header(ofmt_ctx, NULL);
    if (ret < 0) 
	{
        fprintf(fp_log, "9.avformat_write_header() error!!!\n");
        return -1;
    }
	
	//保存编码器ctx
	enc_ctx = codec_ctx;

	//打印时间基帧率相关信息
	fprintf(fp_log, "===========output===========\n");
	fprintf(fp_log, "AVFormatContext:\n");
	fprintf(fp_log, "1.AV_TIME_BASE:%lld\n", AV_TIME_BASE);
	fprintf(fp_log, "2.AV_TIME_BASE_Q:%d/%ld\n", AV_TIME_BASE_Q.num, AV_TIME_BASE_Q.den);
	fprintf(fp_log, "3.start_time:%lld\n", ofmt_ctx->start_time);
	fprintf(fp_log, "4.duration:%ld\n", ofmt_ctx->duration);
	fprintf(fp_log, "5.bit_rate:%lld\n", ofmt_ctx->bit_rate);
	
	fprintf(fp_log, "\nout_stream:\n");
	fprintf(fp_log, "1.time_base:%d/%ld\n", out_stream->time_base.num, out_stream->time_base.den);
	fprintf(fp_log, "2.start_time:%lld\n", out_stream->start_time);
	fprintf(fp_log, "3.duration:%ld\n", out_stream->duration);

	fprintf(fp_log, "\nenc_ctx:\n");
	fprintf(fp_log, "1.time_base:%d/%ld\n", enc_ctx->time_base.num, enc_ctx->time_base.den);
	fprintf(fp_log, "2.pix_fmt:%d\n", enc_ctx->pix_fmt);
	fprintf(fp_log, "3.decodec_id:%d\n", AV_CODEC_ID_H264);
	fprintf(fp_log, "4.width:%d\n", enc_ctx->width);
	fprintf(fp_log, "5.height:%d\n", enc_ctx->height);
	fprintf(fp_log, "===========================\n");

    //av_dump_format(ofmt_ctx, 0, filename, 1);	

    return 0;
}

int encode_write_frame(AVFrame *sws_frame, unsigned int stream_index) 
{
    int ret;
    int got_frame;
    AVPacket enc_pkt;

    enc_pkt.data = NULL;
    enc_pkt.size = 0;
    av_init_packet(&enc_pkt);
   
	//开始编码(这里如果编解码器时间基不一致,这里也需要做转换)
    ret = avcodec_encode_video2(enc_ctx, &enc_pkt, sws_frame, &got_frame);
    if(ret < 0)//编码失败
    return ret;
    if(!got_frame)//往编码器送frame,但是编码器缓存没满
    return 0;
	
    //设置流对应的index
    enc_pkt.stream_index = stream_index;
	
	//时间基转换(encode--->out_stream)
    av_packet_rescale_ts(&enc_pkt, enc_ctx->time_base, ofmt_ctx->streams[stream_index]->time_base);

    //开始封装
    ret = av_interleaved_write_frame(ofmt_ctx, &enc_pkt);
    return ret;
}

int swscale_encode_write_frame(AVFrame *frame, int stream_index)
{
	int ret = 0;
	AVFrame *sws_frame=av_frame_alloc();
	
	int height = dec_ctx->height;
	int width = dec_ctx->width;
	int pix_fmt = dec_ctx->pix_fmt;
	
	//转格式
	struct SwsContext *img_convert_ctx = sws_getContext(width,height,pix_fmt,width,height,AV_PIX_FMT_YUV420P,SWS_BILINEAR,NULL,NULL,NULL);
	
	//分配空间
	unsigned char *sws_buf=(unsigned char *)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_YUV420P,width,height,4));  	
	av_image_fill_arrays((AVPicture*)sws_frame,sws_frame->linesize,sws_buf,AV_PIX_FMT_YUV420P,width,height,4);
	
	//转格式
	sws_scale(img_convert_ctx,
			(const unsigned char* const*)frame->data,
			frame->linesize,
			0,
			height,
			sws_frame->data,
			sws_frame->linesize);
	
	sws_frame->pts = frame->pts;
	sws_frame->pkt_dts = frame->pkt_dts;
	sws_frame->pkt_duration = frame->pkt_duration;
	sws_frame->best_effort_timestamp = frame->best_effort_timestamp;
	sws_frame->format = AV_PIX_FMT_YUV420P;
	sws_frame->width = frame->width;
	sws_frame->height = frame->height;
	sws_frame->pict_type = frame->pict_type;

	//编码写文件
	ret = encode_write_frame(sws_frame, stream_index);

	av_frame_free(&sws_frame);
	av_free(sws_buf);
	sws_freeContext(img_convert_ctx);
	
	
	return ret;	
}

int main(int argc, char *argv[])
{
    int ret;
	AVPacket *packet = NULL;
    AVFrame *frame = NULL;
    int type;
    unsigned int stream_index;
    unsigned int i;
    int got_frame;
	
	//const char *input_file = "video=HP Webcam-101";
	//const char *output_file = "rtmp://169.254.96.183:1935/live/home";
	char *input_file = argv[1];
	char *output_file = argv[2];	

	//日志文件
	fp_log = fopen("log.txt", "wb+");
	if(!fp_log)
	{
		return -1;
	}

	//设备初始化
	avdevice_register_all();
	//网络初始化
	avformat_network_init();

    open_input_file(input_file);
	open_output_file(output_file);

	packet = av_packet_alloc();
    frame = av_frame_alloc();
	
    while(1)
	{
        ret = av_read_frame(ifmt_ctx, packet);
		if(ret<0)
		{
			fprintf(fp_log, "###av_read_frame() error!\n");
			break;
		}

        stream_index = packet->stream_index;
        type = ifmt_ctx->streams[stream_index]->codecpar->codec_type;

		if(type == AVMEDIA_TYPE_VIDEO)
		{
			//转时间基(in_stream--->dec_ctx)
            av_packet_rescale_ts(packet, ifmt_ctx->streams[stream_index]->time_base, dec_ctx->time_base);
			
			//开始解码
            ret = avcodec_decode_video2(dec_ctx, frame, &got_frame, packet);
            if(ret < 0)
			{
				//解码失败
				fprintf(fp_log, "###avcodec_decode_video2() error!\n");
                break;
            }

			//往解码器送packet,但是没有送满
			if(!got_frame)
			{
				fprintf(fp_log, "###not start decode!\n");
				av_frame_unref(frame);
				av_packet_unref(packet);
				continue;
			}
			
			//转格式--->编码--->封装
			ret = swscale_encode_write_frame(frame, stream_index);
            if(ret < 0)
			{
				fprintf(fp_log, "###swscale_encode_write_frame()!\n");
                break;
            }
        }
		av_frame_unref(frame);
        av_packet_unref(packet);
    }

	av_frame_free(&frame);
    av_packet_free(&packet);
	avformat_close_input(&ifmt_ctx);
	fclose(fp_log);

    return 0;
}








