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

static AVFormatContext *ifmt_ctx;
static AVFormatContext *ofmt_ctx;
typedef struct FilteringContext 
{
    AVFilterContext *buffersink_ctx;
    AVFilterContext *buffersrc_ctx;
    AVFilterGraph *filter_graph;
} FilteringContext;
static FilteringContext *filter_ctx;

typedef struct StreamContext 
{
    AVCodecContext *dec_ctx;
    AVCodecContext *enc_ctx;
} StreamContext;
static StreamContext *stream_ctx;

static int open_input_file(const char *filename)
{
    int ret;
    unsigned int i;
	AVInputFormat* input_device = NULL;

	//����ͷ����
#if 0
	avdevice_register_all();
	input_device = av_find_input_format("dshow");
	if(!input_device)
	{
		printf("av_find_input_format()\n");
		return ret;
	}	
#else	
	//���ļ�
    ifmt_ctx = NULL;
    if ((ret = avformat_open_input(&ifmt_ctx, filename, input_device, NULL)) < 0)
	{
        printf("Cannot open input file\n");
        return ret;
    }
#endif

	//��ȡ��
    if ((ret = avformat_find_stream_info(ifmt_ctx, NULL)) < 0) 
	{
        printf("Cannot find stream information\n");
        return ret;
    }

	//���ٱ�������ռ�,һ������Ӧһ����������һ��������
    stream_ctx = av_mallocz_array(ifmt_ctx->nb_streams, sizeof(*stream_ctx));
    if (!stream_ctx)
        return AVERROR(ENOMEM);

    for (i = 0; i < ifmt_ctx->nb_streams; i++)
	{
		AVCodecContext *codec_ctx;
		//��ȡ��	
        AVStream *stream = ifmt_ctx->streams[i];
		//��������Ӧ�Ľ�����
        AVCodec *dec = avcodec_find_decoder(stream->codecpar->codec_id);
        if (!dec) 
		{
            printf("Failed to find decoder for stream #%u\n", i);
            return AVERROR_DECODER_NOT_FOUND;
        }
		
		//���ݽ��������������ctx
        codec_ctx = avcodec_alloc_context3(dec);
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
        
		//�����Ҫ�Ļ�,����Ƶ��Ļ����һЩ������������ﴦ��
        if(codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO || codec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) 
		{
			//��Ƶ����ȡһ��֡��,�����Ҫ�Ļ�
            if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO)
			{
				codec_ctx->framerate = av_guess_frame_rate(ifmt_ctx, stream, NULL);
			}
				
			//�򿪽�����
            ret = avcodec_open2(codec_ctx, dec, NULL);
            if (ret < 0) 
			{
                printf("Failed to open decoder for stream #%u\n", i);
                return ret;
            }
        }
		//���������ctx
        stream_ctx[i].dec_ctx = codec_ctx;
		
		//��ӡʱ���֡�������Ϣ
		printf("stream time_base:%d/%d\n", stream->time_base.num, stream->time_base.den);
		printf("codec_ctx time_base:%d/%d\n", codec_ctx->time_base.num, codec_ctx->time_base.den);
		printf("codec_ctx framerate:%d/%d\n", codec_ctx->framerate.num, codec_ctx->framerate.den);
    }

    av_dump_format(ifmt_ctx, 0, filename, 0);
    return 0;
}

static int open_output_file(const char *filename)
{
    AVStream *out_stream;
    AVStream *in_stream;
    AVCodecContext *dec_ctx, *enc_ctx;
    AVCodec *encoder;
    int ret;
    unsigned int i;

	//������ļ�
    ofmt_ctx = NULL;
    avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, filename);
    if (!ofmt_ctx) 
	{
        printf("Could not create output context\n");
        return AVERROR_UNKNOWN;
    }

	//���������
    for (i = 0; i < ifmt_ctx->nb_streams; i++) 
	{
		//����һ�������
        out_stream = avformat_new_stream(ofmt_ctx, NULL);
        if (!out_stream) 
		{
            printf("Failed allocating output stream\n");
            return AVERROR_UNKNOWN;
        }
		
		//�����������������ctx
        in_stream = ifmt_ctx->streams[i];
        dec_ctx = stream_ctx[i].dec_ctx;

        if(dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO || dec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) 
		{
            //������������óɺͽ�����һ��,Ҳ����ָ��������
            encoder = avcodec_find_encoder(dec_ctx->codec_id);
            if (!encoder) 
			{
                printf("Necessary encoder not found\n");
                return AVERROR_INVALIDDATA;
            }
			//���������ctx
            enc_ctx = avcodec_alloc_context3(encoder);
            if (!enc_ctx) 
			{
                printf("Failed to allocate the encoder context\n");
                return AVERROR(ENOMEM);
            }

			//���������һЩ�������óɺ������ļ�һ��,���ڹ���������
            if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO)
			{
                enc_ctx->height = dec_ctx->height;
                enc_ctx->width = dec_ctx->width;
				
				//PAR ���� Pixel Aspect Ratio ���غ��ݱȡ���ʾÿ�����صĿ���볤�ȵı�ֵ��������Ϊÿ�����ز��������εġ�
				//DAR ���� Display Aspect Ratio ��ʾ���ݱȡ�������ʾ��ͼ���ڳ��ȵ�λ�ϵĺ��ݱȡ�
				//SAR ���� Sample Aspect Ratio �������ݱȡ���ʾ��������ص�������������ص����ı�ֵ��
                enc_ctx->sample_aspect_ratio = dec_ctx->sample_aspect_ratio;
				printf("pix-x:%d  pix-y:%d\n", enc_ctx->sample_aspect_ratio.num, enc_ctx->sample_aspect_ratio.den);
                
				//��������ظ�ʽ����ѡ�������ö�ٵĵ�һ�ָ�ʽ,Ҳ�����Լ�ѡ��
                if (encoder->pix_fmts)
                    enc_ctx->pix_fmt = encoder->pix_fmts[0];
                else
                    enc_ctx->pix_fmt = dec_ctx->pix_fmt;
                
				//���ݽ�������֡�������������ʱ���
                enc_ctx->time_base = av_inv_q(dec_ctx->framerate);
            } 
			else 
			{
                enc_ctx->sample_rate = dec_ctx->sample_rate;
                enc_ctx->channel_layout = dec_ctx->channel_layout;
                enc_ctx->channels = av_get_channel_layout_nb_channels(enc_ctx->channel_layout);
                /* take first format from list of supported formats */
                enc_ctx->sample_fmt = encoder->sample_fmts[0];
                enc_ctx->time_base = (AVRational){1, enc_ctx->sample_rate};
            }

			//�ļ�Ԥ����־��������ʱû�о�
            if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
                enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

            //�򿪱�����,����������һЩ��ʼ���Ĺ���,�����������������ò������ݸ�������
            ret = avcodec_open2(enc_ctx, encoder, NULL);
            if (ret < 0) 
			{
                printf("Cannot open video encoder for stream #%u\n", i);
                return ret;
            }
			
			//��ʼ��������
            ret = avcodec_parameters_from_context(out_stream->codecpar, enc_ctx);
            if (ret < 0) 
			{
                printf("Failed to copy encoder parameters to output stream #%u\n", i);
                return ret;
            }
			
			//�����ʱ�����ֵ
            out_stream->time_base = enc_ctx->time_base;
			
			//���������ctx
            stream_ctx[i].enc_ctx = enc_ctx;
			
        } //������һЩ�������Ĳ���
		else if (dec_ctx->codec_type == AVMEDIA_TYPE_UNKNOWN) 
		{
            printf("Elementary stream #%d is of unknown type, cannot proceed\n", i);
            return AVERROR_INVALIDDATA;
        } 
		else
		{
            /* if this stream must be remuxed */
            ret = avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar);
            if (ret < 0) 
			{
                printf("Copying parameters for stream #%u failed\n", i);
                return ret;
            }
            out_stream->time_base = in_stream->time_base;
        }
		//��ӡʱ���֡�������Ϣ
		printf("out_stream time_base:%d/%d\n", out_stream->time_base.num, out_stream->time_base.den);
		printf("enc_ctx time_base:%d/%d\n", enc_ctx->time_base.num, enc_ctx->time_base.den);
		printf("enc_ctx framerate:%d/%d\n", enc_ctx->framerate.num, enc_ctx->framerate.den);
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

static int init_filter(FilteringContext* fctx, AVCodecContext *dec_ctx,
        AVCodecContext *enc_ctx, const char *filter_spec)
{
    char args[512];
    int ret = 0;
    const AVFilter *buffersrc = NULL;
    const AVFilter *buffersink = NULL;
    AVFilterContext *buffersrc_ctx = NULL;
    AVFilterContext *buffersink_ctx = NULL;
    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs  = avfilter_inout_alloc();
    AVFilterGraph *filter_graph = avfilter_graph_alloc();

    if (!outputs || !inputs || !filter_graph) 
	{
        ret = AVERROR(ENOMEM);
        goto end;
    }

    if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) 
	{
        buffersrc = avfilter_get_by_name("buffer");
        buffersink = avfilter_get_by_name("buffersink");
        if (!buffersrc || !buffersink) 
		{
            av_log(NULL, AV_LOG_ERROR, "filtering source or sink element not found\n");
            ret = AVERROR_UNKNOWN;
            goto end;
        }

        snprintf(args, sizeof(args),
                "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
                dec_ctx->width, dec_ctx->height, dec_ctx->pix_fmt,
                dec_ctx->time_base.num, dec_ctx->time_base.den,
                dec_ctx->sample_aspect_ratio.num,
                dec_ctx->sample_aspect_ratio.den);

        ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in", args, NULL, filter_graph);
        if (ret < 0)
		{
            av_log(NULL, AV_LOG_ERROR, "Cannot create buffer source\n");
            goto end;
        }

        ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out", NULL, NULL, filter_graph);
        if (ret < 0) 
		{
            av_log(NULL, AV_LOG_ERROR, "Cannot create buffer sink\n");
            goto end;
        }

        ret = av_opt_set_bin(buffersink_ctx, "pix_fmts", (uint8_t*)&enc_ctx->pix_fmt, sizeof(enc_ctx->pix_fmt), AV_OPT_SEARCH_CHILDREN);
        if (ret < 0) 
		{
            av_log(NULL, AV_LOG_ERROR, "Cannot set output pixel format\n");
            goto end;
        }
    } 
	else if (dec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) 
	{
        buffersrc = avfilter_get_by_name("abuffer");
        buffersink = avfilter_get_by_name("abuffersink");
        if (!buffersrc || !buffersink) 
		{
            av_log(NULL, AV_LOG_ERROR, "filtering source or sink element not found\n");
            ret = AVERROR_UNKNOWN;
            goto end;
        }

        if (!dec_ctx->channel_layout)
            dec_ctx->channel_layout =
                av_get_default_channel_layout(dec_ctx->channels);
        snprintf(args, sizeof(args),
                "time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=0x%"PRIx64,
                dec_ctx->time_base.num, dec_ctx->time_base.den, dec_ctx->sample_rate,
                av_get_sample_fmt_name(dec_ctx->sample_fmt),
                dec_ctx->channel_layout);
        ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
                args, NULL, filter_graph);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot create audio buffer source\n");
            goto end;
        }

        ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
                NULL, NULL, filter_graph);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot create audio buffer sink\n");
            goto end;
        }

        ret = av_opt_set_bin(buffersink_ctx, "sample_fmts",
                (uint8_t*)&enc_ctx->sample_fmt, sizeof(enc_ctx->sample_fmt),
                AV_OPT_SEARCH_CHILDREN);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot set output sample format\n");
            goto end;
        }

        ret = av_opt_set_bin(buffersink_ctx, "channel_layouts",
                (uint8_t*)&enc_ctx->channel_layout,
                sizeof(enc_ctx->channel_layout), AV_OPT_SEARCH_CHILDREN);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot set output channel layout\n");
            goto end;
        }

        ret = av_opt_set_bin(buffersink_ctx, "sample_rates",
                (uint8_t*)&enc_ctx->sample_rate, sizeof(enc_ctx->sample_rate),
                AV_OPT_SEARCH_CHILDREN);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot set output sample rate\n");
            goto end;
        }
    } else {
        ret = AVERROR_UNKNOWN;
        goto end;
    }

    /* Endpoints for the filter graph. */
    outputs->name       = av_strdup("in");
    outputs->filter_ctx = buffersrc_ctx;
    outputs->pad_idx    = 0;
    outputs->next       = NULL;

    inputs->name       = av_strdup("out");
    inputs->filter_ctx = buffersink_ctx;
    inputs->pad_idx    = 0;
    inputs->next       = NULL;

    if (!outputs->name || !inputs->name) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    if ((ret = avfilter_graph_parse_ptr(filter_graph, filter_spec,
                    &inputs, &outputs, NULL)) < 0)
        goto end;

    if ((ret = avfilter_graph_config(filter_graph, NULL)) < 0)
        goto end;

    /* Fill FilteringContext */
    fctx->buffersrc_ctx = buffersrc_ctx;
    fctx->buffersink_ctx = buffersink_ctx;
    fctx->filter_graph = filter_graph;

end:
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);

    return ret;
}

static int init_filters(void)
{
    const char *filter_spec;
    unsigned int i;
    int ret;
    filter_ctx = av_malloc_array(ifmt_ctx->nb_streams, sizeof(*filter_ctx));
    if (!filter_ctx)
        return AVERROR(ENOMEM);

    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        filter_ctx[i].buffersrc_ctx  = NULL;
        filter_ctx[i].buffersink_ctx = NULL;
        filter_ctx[i].filter_graph   = NULL;
        if (!(ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO
                || ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO))
            continue;


        if (ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
            filter_spec = "null"; /* passthrough (dummy) filter for video */
        else
            filter_spec = "anull"; /* passthrough (dummy) filter for audio */
        ret = init_filter(&filter_ctx[i], stream_ctx[i].dec_ctx,
                stream_ctx[i].enc_ctx, filter_spec);
        if (ret)
            return ret;
    }
    return 0;
}

static int encode_write_frame(AVFrame *filt_frame, unsigned int stream_index, int *got_frame) 
{
    int ret;
    int got_frame_local;
    AVPacket enc_pkt;
    int (*enc_func)(AVCodecContext *, AVPacket *, const AVFrame *, int *) =
        (ifmt_ctx->streams[stream_index]->codecpar->codec_type ==
         AVMEDIA_TYPE_VIDEO) ? avcodec_encode_video2 : avcodec_encode_audio2;

    if (!got_frame)
        got_frame = &got_frame_local;

    printf("Encoding frame\n");
    /* encode filtered frame */
    enc_pkt.data = NULL;
    enc_pkt.size = 0;
    av_init_packet(&enc_pkt);
	
	//��ʼ����
    ret = enc_func(stream_ctx[stream_index].enc_ctx, &enc_pkt, filt_frame, got_frame);
    av_frame_free(&filt_frame);
    if (ret < 0)
        return ret;
    if (!(*got_frame))
        return 0;

    //��������Ӧ��index
    enc_pkt.stream_index = stream_index;
	
	//ʱ���ת��enc_ctx.time_base---->ofmt_ctx.time_base
    av_packet_rescale_ts(&enc_pkt,
                         stream_ctx[stream_index].enc_ctx->time_base,
                         ofmt_ctx->streams[stream_index]->time_base);

    printf("Muxing frame\n");
	
    //д�ļ�,��ʼ��װ
    ret = av_interleaved_write_frame(ofmt_ctx, &enc_pkt);
    return ret;
}

static int filter_encode_write_frame(AVFrame *frame, unsigned int stream_index)
{
    int ret;
    AVFrame *filt_frame;

	//frame�͵�������
    printf("Pushing decoded frame to filters\n");
    ret = av_buffersrc_add_frame_flags(filter_ctx[stream_index].buffersrc_ctx, frame, 0);
    if (ret < 0) 
	{
        printf("Error while feeding the filtergraph\n");
        return ret;
    }

    //�ӹ�������frame
    while (1) 
	{
        filt_frame = av_frame_alloc();
        if (!filt_frame) 
		{
            ret = AVERROR(ENOMEM);
            break;
        }
        printf("Pulling filtered frame from filters\n");
        ret = av_buffersink_get_frame(filter_ctx[stream_index].buffersink_ctx, filt_frame);
        if (ret < 0) 
		{
            /* if no more frames for output - returns AVERROR(EAGAIN)
             * if flushed and no more frames for output - returns AVERROR_EOF
             * rewrite retcode to 0 to show it as normal procedure completion
             */
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                ret = 0;
            av_frame_free(&filt_frame);
            break;
        }

        filt_frame->pict_type = AV_PICTURE_TYPE_NONE;
		
		//filt_frame,Ȼ�����,д���ļ�
        ret = encode_write_frame(filt_frame, stream_index, NULL);
        if (ret < 0)
            break;
    }

    return ret;
}

static int flush_encoder(unsigned int stream_index)
{
    int ret;
    int got_frame;

    if (!(stream_ctx[stream_index].enc_ctx->codec->capabilities &
                AV_CODEC_CAP_DELAY))
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
    int (*dec_func)(AVCodecContext *, AVFrame *, int *, const AVPacket *);


    if ((ret = open_input_file("1.mp4")) < 0)
        goto end;
    if ((ret = open_output_file("2.mp4")) < 0)
        goto end;
	
	
	//printf("out_stream time_base:%d/%d\n", out_stream->time_base.num, out_stream->time_base.den);
	printf("enc_ctx time_base:%d/%d\n", stream_ctx[0].enc_ctx->time_base.num, stream_ctx[0].enc_ctx->time_base.den);
	printf("enc_ctx framerate:%d/%d\n", stream_ctx[0].enc_ctx->framerate.num, stream_ctx[0].enc_ctx->framerate.den);
	
	//printf("stream time_base:%d/%d\n", stream->time_base.num, stream->time_base.den);
	printf("codec_ctx time_base:%d/%d\n", stream_ctx[0].dec_ctx->time_base.num, stream_ctx[0].dec_ctx->time_base.den);
	printf("codec_ctx framerate:%d/%d\n", stream_ctx[0].dec_ctx->framerate.num, stream_ctx[0].dec_ctx->framerate.den);

	stream_ctx[0].enc_ctx->time_base.num = stream_ctx[0].dec_ctx->time_base.num;
	stream_ctx[0].enc_ctx->time_base.den = stream_ctx[0].dec_ctx->time_base.den;
	stream_ctx[0].enc_ctx->framerate.num = stream_ctx[0].dec_ctx->framerate.num;
	stream_ctx[0].enc_ctx->framerate.den = stream_ctx[0].dec_ctx->framerate.den;

	
	system("pause");
    if ((ret = init_filters()) < 0)
        goto end;
	
    /* read all packets */
    while (1) 
	{
        if ((ret = av_read_frame(ifmt_ctx, &packet)) < 0)
            break;
        stream_index = packet.stream_index;
        type = ifmt_ctx->streams[packet.stream_index]->codecpar->codec_type;

        if (filter_ctx[stream_index].filter_graph) 
		{
            frame = av_frame_alloc();
            if (!frame) 
			{
                ret = AVERROR(ENOMEM);
                break;
            }
			
			//��ifmt_ctx�������ʱ���ת��codec�������ʱ���
            av_packet_rescale_ts(&packet,
                                 ifmt_ctx->streams[stream_index]->time_base,
                                 stream_ctx[stream_index].dec_ctx->time_base);
								 
            dec_func = (type == AVMEDIA_TYPE_VIDEO) ? avcodec_decode_video2 : avcodec_decode_audio4;
			
			//��ʼ����
            ret = dec_func(stream_ctx[stream_index].dec_ctx, frame, &got_frame, &packet);
            if (ret < 0) 
			{
                av_frame_free(&frame);
                printf("Decoding failed\n");
                break;
            }

			//����ɹ�
            if (got_frame)
			{
				//����дһ��frame pts,Ȼ���͵�������
                frame->pts = frame->best_effort_timestamp;
                ret = filter_encode_write_frame(frame, stream_index);
                av_frame_free(&frame);
                if (ret < 0)
					goto end;
            } 
			else 
			{
                av_frame_free(&frame);
            }
        } 
		else //����Ҫ�˾���ת��ֱ��д���ļ�����
		{
            
            av_packet_rescale_ts(&packet,
                                 ifmt_ctx->streams[stream_index]->time_base,
                                 ofmt_ctx->streams[stream_index]->time_base);

            ret = av_interleaved_write_frame(ofmt_ctx, &packet);
            if (ret < 0)
                goto end;
        }
        av_packet_unref(&packet);
    }

    //��ϴ�������ͱ�����
    for (i = 0; i < ifmt_ctx->nb_streams; i++) 
	{
        //��ϴ������
        if (!filter_ctx[i].filter_graph)
            continue;
        ret = filter_encode_write_frame(NULL, i);
        if (ret < 0) 
		{
            printf("Flushing filter failed\n");
            goto end;
        }

        //��ϴ������
        ret = flush_encoder(i);
        if (ret < 0) 
		{
            printf("Flushing encoder failed\n");
            goto end;
        }
    }
	
	//д����ļ�
    av_write_trailer(ofmt_ctx);
	
end:
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

    return ret ? 1 : 0;
}

