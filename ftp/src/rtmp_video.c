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
	
	//���û���,Ĭ�ϴ�С3041280
	av_dict_set_int(&options, "rtbufsize", 3041280 * 100, 0);

	//���豸
	AVInputFormat *input_device = av_find_input_format("dshow");
	if(!input_device)
	{
		fprintf(fp_log, "1.av_find_input_format() error!!!\n");
		return ret;
	}	

	//������ͷ
    if((ret = avformat_open_input(&ifmt_ctx, filename, input_device, &options)) < 0)
	{
        fprintf(fp_log, "2.avformat_open_input() error!!!\n");
        return ret;
    }

	//��ȡ��
    if((ret = avformat_find_stream_info(ifmt_ctx, NULL)) < 0)
	{
        fprintf(fp_log, "3.avformat_find_stream_info() error!!!\n");
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
        fprintf(fp_log, "4.not find video index error!!!\n");
        return ret;	
	}
	
	AVStream *stream = ifmt_ctx->streams[video_index];

	//��������Ӧ�Ľ����� AV_CODEC_ID_RAWVIDEOԭʼѹ����ʽ 13
	AVCodec *dec = avcodec_find_decoder(stream->codecpar->codec_id);
	if(!dec) 
	{
		fprintf(fp_log, "5.avcodec_find_decoder() error!!!\n");
		return -1;
	}			
	
	//���ݽ��������������ctx
	AVCodecContext *codec_ctx = avcodec_alloc_context3(dec);
	if (!codec_ctx)
	{
		fprintf(fp_log, "6.avcodec_alloc_context3() error!!!\n");
		return -1;
	}
	
	//copy���������ݵ�������ctx
	ret = avcodec_parameters_to_context(codec_ctx, stream->codecpar);
	if(ret < 0)
	{
		fprintf(fp_log, "7.avcodec_parameters_to_context() error!!!\n");
		return ret;
	}

	//��Ƶ����ȡһ��֡��,�����Ҫ�Ļ�
	codec_ctx->framerate = av_guess_frame_rate(ifmt_ctx, stream, NULL);
	
	//�򿪽�����
    ret = avcodec_open2(codec_ctx, dec, NULL);
    if(ret < 0) 
	{
        fprintf(fp_log, "8.avcodec_open2() error!!!\n");
        return ret;
    }
	
	//���������ctx
	dec_ctx = codec_ctx;

	//��ӡʱ���֡�������Ϣ
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
	
	//������ļ�
    avformat_alloc_output_context2(&ofmt_ctx, NULL, "flv", filename);
    if(!ofmt_ctx) 
	{
		fprintf(fp_log, "1.avformat_alloc_output_context2() error!!!\n");
        return -1;
    }

	//���������
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
	
	//����һ�������
	AVStream *out_stream = avformat_new_stream(ofmt_ctx, NULL);
	if (!out_stream)
	{
		fprintf(fp_log, "3.avformat_new_stream() error!!!\n");
		return -1;
	}
	
	//������������óɺͽ�����һ��,Ҳ����ָ��������,AV_CODEC_ID_H264
	AVCodec *encoder = avcodec_find_encoder(AV_CODEC_ID_H264);
	if (!encoder) 
	{
		fprintf(fp_log, "4.avcodec_find_encoder() error!!!\n");
		return -1;
	}
	//���������ctx
	AVCodecContext *codec_ctx = avcodec_alloc_context3(encoder);
	if (!codec_ctx) 
	{
		fprintf(fp_log, "5.avcodec_alloc_context3() error!!!\n");
		return -1;
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
	
	//���ݽ�������֡�������������ʱ���
	codec_ctx->time_base = av_inv_q(dec_ctx->framerate);
	
	//������B֡,���ڴ���pts��dts
	codec_ctx->max_b_frames = 0;	
	
	//�ļ�Ԥ����־����ʱû�о�
	if(ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
	codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

	//�򿪱�����,����������һЩ��ʼ���Ĺ���
	ret = avcodec_open2(codec_ctx, encoder, NULL);
	if (ret < 0) 
	{
		fprintf(fp_log, "6.avcodec_open2() error!!!\n");
		return -1;
	}
	
	//��ʼ��������
	ret = avcodec_parameters_from_context(out_stream->codecpar, codec_ctx);
	if (ret < 0) 
	{
		fprintf(fp_log, "7.avcodec_parameters_from_context() error!!!\n");
		return -1;
	}
	
	//�����ʱ�����ֵ
	out_stream->time_base = codec_ctx->time_base;
	
	//������ļ�
    if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) 
	{
        ret = avio_open(&ofmt_ctx->pb, filename, AVIO_FLAG_WRITE);
        if (ret < 0) 
		{
            fprintf(fp_log, "8.avio_open() error!!!\n");
            return -1;
        }
    }

    //д�ļ�ͷ
    ret = avformat_write_header(ofmt_ctx, NULL);
    if (ret < 0) 
	{
        fprintf(fp_log, "9.avformat_write_header() error!!!\n");
        return -1;
    }
	
	//���������ctx
	enc_ctx = codec_ctx;

	//��ӡʱ���֡�������Ϣ
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
   
	//��ʼ����(��������������ʱ�����һ��,����Ҳ��Ҫ��ת��)
    ret = avcodec_encode_video2(enc_ctx, &enc_pkt, sws_frame, &got_frame);
    if(ret < 0)//����ʧ��
    return ret;
    if(!got_frame)//����������frame,���Ǳ���������û��
    return 0;
	
    //��������Ӧ��index
    enc_pkt.stream_index = stream_index;
	
	//ʱ���ת��(encode--->out_stream)
    av_packet_rescale_ts(&enc_pkt, enc_ctx->time_base, ofmt_ctx->streams[stream_index]->time_base);

    //��ʼ��װ
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
	
	//ת��ʽ
	struct SwsContext *img_convert_ctx = sws_getContext(width,height,pix_fmt,width,height,AV_PIX_FMT_YUV420P,SWS_BILINEAR,NULL,NULL,NULL);
	
	//����ռ�
	unsigned char *sws_buf=(unsigned char *)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_YUV420P,width,height,4));  	
	av_image_fill_arrays((AVPicture*)sws_frame,sws_frame->linesize,sws_buf,AV_PIX_FMT_YUV420P,width,height,4);
	
	//ת��ʽ
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

	//����д�ļ�
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

	//��־�ļ�
	fp_log = fopen("log.txt", "wb+");
	if(!fp_log)
	{
		return -1;
	}

	//�豸��ʼ��
	avdevice_register_all();
	//�����ʼ��
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
			//תʱ���(in_stream--->dec_ctx)
            av_packet_rescale_ts(packet, ifmt_ctx->streams[stream_index]->time_base, dec_ctx->time_base);
			
			//��ʼ����
            ret = avcodec_decode_video2(dec_ctx, frame, &got_frame, packet);
            if(ret < 0)
			{
				//����ʧ��
				fprintf(fp_log, "###avcodec_decode_video2() error!\n");
                break;
            }

			//����������packet,����û������
			if(!got_frame)
			{
				fprintf(fp_log, "###not start decode!\n");
				av_frame_unref(frame);
				av_packet_unref(packet);
				continue;
			}
			
			//ת��ʽ--->����--->��װ
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








