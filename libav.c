#include <caml/mlvalues.h>
#include <caml/alloc.h>
#include <caml/memory.h>
#include <caml/custom.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/pixfmt.h>

void avframe_finalize(value v)
{
    AVFrame *frame = *(AVFrame**)Data_custom_val(v);
    av_frame_free(&frame);
}

static struct custom_operations avframe_ops = {
    identifier: "avframe handling",
    finalize: avframe_finalize,
    compare: custom_compare_default,
    hash: custom_hash_default,
    serialize: custom_serialize_default,
    deserialize: custom_deserialize_default 
};

static void write_image_in(AVFrame *frame, char *fname)
{
    AVOutputFormat *fmt;
    AVFormatContext *oc;
    AVCodecContext *avctx;
    AVCodec *codec;
    AVStream *st;
    AVPacket pkt = {0};
    int ret, got_pkt;

    fmt = av_guess_format(NULL, fname, NULL);
    if (!fmt) {
        fprintf(stderr, "Unable to deduce output format\n");
        return;
    }
    oc = avformat_alloc_context();
    if (!oc) {
        fprintf(stderr, "Unable to alloc output context\n");
        return;
    }
    oc->oformat = fmt;
    snprintf(oc->filename, sizeof(oc->filename), "%s", fname);
    if (fmt->video_codec == AV_CODEC_ID_NONE) {
        fprintf(stderr, "Unable to find codec\n");
        goto out_fail;
    }
    fmt->video_codec = av_guess_codec(fmt, NULL, fname, NULL, AVMEDIA_TYPE_VIDEO);
    codec = avcodec_find_encoder(fmt->video_codec);
    if (!codec) {
        fprintf(stderr, "Codec not found.\n");
        goto out_fail;
    }
    st = avformat_new_stream(oc, codec);
    if (!st) {
        fprintf(stderr, "Unable to alloc stream\n");
        goto out_fail;
    }
    avctx = st->codec;
    avctx->width = frame->width;
    avctx->height = frame->height;
    avctx->pix_fmt = frame->format;
    avctx->time_base.den = 10;
    avctx->time_base.num = 1;
    if (avcodec_open2(avctx, NULL, NULL) < 0) {
        fprintf(stderr, "Unable to open codec\n");
        goto out_fail;
    }

    if (!(fmt->flags & AVFMT_NOFILE)) {
        if (avio_open(&oc->pb, fname, AVIO_FLAG_WRITE) < 0) {
            fprintf(stderr, "Unable to open file\n");
            goto out_fail;
        }
    }

    avformat_write_header(oc, NULL);

    av_init_packet(&pkt);
    ret = avcodec_encode_video2(avctx, &pkt, frame, &got_pkt);
    if (!ret && got_pkt && pkt.size) {
        pkt.stream_index = st->index;
        if (av_interleaved_write_frame(oc, &pkt) != 0) {
            fprintf(stderr, "Error while writing packet\n");
            goto out_fail;
        }
    } else {
        fprintf(stderr, "Error encoding...\n");
        goto out_fail;
    }
out_fail:
    avformat_free_context(oc);
}

CAMLprim value
write_image(value v, value fname)
{
    AVFrame *src = *(AVFrame**)Data_custom_val(v);
    write_image_in(src, String_val(fname));
    return Val_unit;
}

static void rgbline2ocaml(AVFrame *src, value img, int line)
{
    int i;
    uint8_t *d = src->data[0] + src->linesize[0]*line;

    CAMLparam1(img);
    CAMLlocal1(arr);
    arr = caml_alloc(src->width, 0);
    caml_modify(&Field(img, line), arr);

    for (i = 0; i < src->width; i++) {
        int rgb = d[0] << 16 | d[1] << 8 | d[2];
        caml_modify(&Field(arr, i), Val_int(rgb));
        d += 3;
    }
    CAMLreturn0;
}

CAMLprim value frame2ocaml(value v)
{
    int i;
    CAMLparam1(v);
    CAMLlocal1(img);
    AVFrame *src = *(AVFrame**)Data_custom_val(v);
    img = caml_alloc(src->height, 0);
    for (i = 0; i < src->height; i++) rgbline2ocaml(src, img, i);
    CAMLreturn(img);
}

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

    CAMLparam1(str);
    CAMLlocal1(ret);

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

    ret = caml_alloc_custom(&avframe_ops, sizeof(AVFrame**), 0, 1);
    memcpy(Data_custom_val(ret), &rgb, sizeof(AVFrame**));

    av_free_packet(&pkt);
    sws_freeContext(sws);
    av_frame_free(&frame);
    avformat_close_input(&ic);
    CAMLreturn(ret);
}
