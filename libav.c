#include <caml/mlvalues.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/pixfmt.h>

CAMLprim value
get_image(value str)
{
    AVFormatContext *ic = NULL;
    AVCodecContext *avctx;
    AVCodec *codec;
    AVPacket pkt;
    AVFrame *frame = av_frame_alloc();
    AVFrame *rgb = av_frame_alloc();
    struct SwsContext *sws = NULL;
    int i, err;

    av_register_all();
    err = avformat_open_input(&ic, String_val(str),  NULL, NULL);
    if (err < 0) printf("error!\n");
    avformat_find_stream_info(ic, NULL);
    for (i = 0; i < ic->nb_streams; i++) {
        avctx = ic->streams[i]->codec;
        codec = avcodec_find_decoder(avctx->codec_id);
        if (!codec || avcodec_open2(avctx, codec, NULL)) printf("error opening!\n");
        if (AVMEDIA_TYPE_VIDEO == avctx->codec_type && !sws) {
            int w = avctx->width, h = avctx->height;
            sws = sws_getContext(w, h, avctx->pix_fmt, w, h, PIX_FMT_RGB24, SWS_BICUBIC, NULL, NULL, 0);
            rgb->format = AV_PIX_FMT_RGB24;
            rgb->width = w;
            rgb->height = h;
            av_frame_get_buffer(rgb, 8);
        }
    }
    av_init_packet(&pkt);
    err = av_read_frame(ic, &pkt);
    if (err < 0) {
        if (err == AVERROR_EOF || ic->pb && ic->pb->eof_reached) {
            printf("eof reached\n");
        }
        if (ic->pb && ic->pb->error) {
            printf("pb_error\n");
        }
    }

    avctx = ic->streams[pkt.stream_index]->codec;
    if (avctx->codec_type == AVMEDIA_TYPE_VIDEO) {
        int got_pic;
        avcodec_decode_video2(avctx, frame, &got_pic, &pkt);
        if (got_pic) {
            sws_scale(sws, (const uint8_t* const*)frame->data, frame->linesize, 0, avctx->height, rgb->data, rgb->linesize);
            av_frame_unref(frame);
        }
    }

    av_free_packet(&pkt);
    sws_freeContext(sws);
    av_frame_free(&frame);
    av_frame_free(&rgb);
    avformat_close_input(&ic);
    return Val_unit;
}
