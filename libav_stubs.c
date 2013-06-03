#include <caml/mlvalues.h>
#include <caml/alloc.h>
#include <caml/memory.h>
#include <caml/custom.h>
#include <caml/fail.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/pixfmt.h>

static value* eof_exception = NULL;

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

typedef struct _AVInput {
    AVFormatContext *ic;
    struct SwsContext *sws;
    int src_h, w, h, pixfmt; // swscale stuff
} AVInput;

void avinput_finalize(value v)
{
    AVInput *in = (AVInput*)Data_custom_val(v);
    AVFormatContext *ic = in->ic;
    if (in->sws) sws_freeContext(in->sws);
    avformat_close_input(&ic);
}

static struct custom_operations avinput_ops = {
    identifier: "avinput handling",
    finalize: avinput_finalize,
    compare: custom_compare_default,
    hash: custom_hash_default,
    serialize: custom_serialize_default,
    deserialize: custom_deserialize_default
};

typedef enum {OK, MUXER_NF, FMT_ALLOC, CODEC_NF, STREAM_ALLOC,
    CODEC_OPEN, FILE_NF, WRITE_ERR, ENCODE_ERR} write_res_t;
static write_res_t write_image_in(AVFrame *frame, char *fname)
{
    AVOutputFormat *fmt;
    AVFormatContext *oc;
    AVCodecContext *avctx;
    AVCodec *codec;
    AVStream *st;
    AVPacket pkt = {0};
    int ret, got_pkt;

    fmt = av_guess_format(NULL, fname, NULL);
    if (!fmt) return MUXER_NF;
    oc = avformat_alloc_context();
    if (!oc) return FMT_ALLOC;
    oc->oformat = fmt;
    snprintf(oc->filename, sizeof(oc->filename), "%s", fname);
    fmt->video_codec = av_guess_codec(fmt, NULL, fname, NULL, AVMEDIA_TYPE_VIDEO);
    codec = avcodec_find_encoder(fmt->video_codec);
    if (!codec) return CODEC_NF;
    st = avformat_new_stream(oc, codec);
    if (!st) {
        ret = STREAM_ALLOC;
        goto out_fail;
    }
    avctx = st->codec;
    avctx->width = frame->width;
    avctx->height = frame->height;
    avctx->pix_fmt = frame->format;
    avctx->time_base.den = 1;
    avctx->time_base.num = 1;
    if (avcodec_open2(avctx, NULL, NULL) < 0) {
        ret = CODEC_NF;
        goto out_fail;
    }

    if (!(fmt->flags & AVFMT_NOFILE)) {
        if (avio_open(&oc->pb, fname, AVIO_FLAG_WRITE) < 0) {
            ret = FILE_NF;
            goto out_fail;
        }
    }

    avformat_write_header(oc, NULL);

    av_init_packet(&pkt);
    ret = avcodec_encode_video2(avctx, &pkt, frame, &got_pkt);
    if (!ret && got_pkt && pkt.size) {
        pkt.stream_index = st->index;
        if (av_interleaved_write_frame(oc, &pkt) != 0) {
            ret = WRITE_ERR;
            goto out_fail;
        }
    } else {
        ret = ENCODE_ERR;
        goto out_fail;
    }
    av_write_trailer(oc);
    return OK;
out_fail:
    avformat_free_context(oc);
    return ret;
}

value handle_write_res(write_res_t res)
{
    switch(res) {
    case MUXER_NF:
        caml_failwith("Unable to deduce output format");
    case FMT_ALLOC:
        caml_failwith("Unable to alloc output context");
    case CODEC_NF:
        caml_failwith("Unable to find codec");
    case STREAM_ALLOC:
        caml_failwith("Unable to alloc stream");
    case FILE_NF:
        caml_failwith("Unable to open file");
    case WRITE_ERR:
        caml_failwith("Error while writing packet");
    case ENCODE_ERR:
        caml_failwith("Error encoding");
    default: ;
    }
    return Val_unit;
}

CAMLprim value
write_image(value v, value fname)
{
    AVFrame *src = *(AVFrame**)Data_custom_val(v);
    write_res_t res = write_image_in(src, String_val(fname));
    return handle_write_res(res);
}

CAMLprim value
ocaml2frame(value img)
{
    int i, j;
    CAMLparam1(img);
    CAMLlocal2(row, framev);
    AVFrame *frame = av_frame_alloc();
    row = Field(img, 0);
    frame->format = AV_PIX_FMT_RGB24;
    frame->height = Wosize_val(img);
    frame->width = Wosize_val(row);
    av_frame_get_buffer(frame, 8);
    for (i = 0; i < frame->height; i++) {
        row = Field(img, i);
        uint8_t *data = frame->data[0] + frame->linesize[0]*i;
        for (j = 0; j < frame->width; j++) {
            int rgb = Int_val(Field(row, j));
            *data++ = rgb >> 16;
            *data++ = (rgb >> 8) & 0xFF;
            *data++ = rgb & 0xFF;
        }
    }
    framev = caml_alloc_custom(&avframe_ops, sizeof(AVFrame**), 0, 1);
    memcpy(Data_custom_val(framev), &frame, sizeof(AVFrame**));
    CAMLreturn(framev);
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
open_input(value str)
{
    AVFormatContext *ic = NULL;
    AVCodecContext *avctx;
    AVCodec *codec;
    AVInput *avi;
    int i, err;

    CAMLparam1(str);
    CAMLlocal1(ret);

    err = avformat_open_input(&ic, String_val(str),  NULL, NULL);
    if (err < 0)
        caml_failwith("Unable to open input context");
    avformat_find_stream_info(ic, NULL);
    for (i = 0; i < ic->nb_streams; i++) {
        avctx = ic->streams[i]->codec;
        codec = avcodec_find_decoder(avctx->codec_id);
        if (!codec || avcodec_open2(avctx, codec, NULL))
            caml_failwith("Unable to open codec");
    }

    ret = caml_alloc_custom(&avinput_ops, sizeof(AVInput), 0, 1);
    avi = (AVInput*)Data_custom_val(ret);
    memset(avi, 0, sizeof(AVInput));
    avi->ic = ic;
    CAMLreturn(ret);
}

static int pixfmt_map[] = {
    PIX_FMT_YUV420P,
    PIX_FMT_RGB24
};

static int get_pixfmt(value pixfmt)
{
    int v = Int_val(pixfmt);
    if (v < 0 || v >= sizeof(pixfmt_map)/sizeof(int)) return -1;
    return pixfmt_map[v];
}

CAMLprim value
set_swscale(value avinput, value pixfmt, value w, value h)
{
    CAMLparam4(avinput, pixfmt, w, h);
    AVInput *avi = (AVInput*)Data_custom_val(avinput);
    AVFormatContext *ic = avi->ic;
    int i, libav_pixfmt = get_pixfmt(pixfmt);
    if (-1 == libav_pixfmt)
        caml_failwith("set_swscale: Invalid pixel format");
    avi->w = Int_val(w);
    avi->h = Int_val(h);
    avi->pixfmt = libav_pixfmt;
    if (avi->sws) sws_freeContext(avi->sws);
    for (i = 0; i < ic->nb_streams; i++) {
        AVCodecContext *avctx = ic->streams[i]->codec;
        if (AVMEDIA_TYPE_VIDEO == avctx->codec_type) {
            avi->src_h = avctx->height;
            avi->sws = sws_getContext(avctx->width, avctx->height,
                avctx->pix_fmt, avi->w, avi->h,
                libav_pixfmt, SWS_BICUBIC, NULL, NULL, 0);
            break;
        }
    }
    if (i == ic->nb_streams)
        caml_failwith("set_swscale: No video streams");
    CAMLreturn(Val_unit);
}

CAMLprim value
get_frame(value avinput)
{
    CAMLparam1(avinput);
    CAMLlocal1(ret);
    AVInput *avi = (AVInput*)Data_custom_val(avinput);
    AVFormatContext *ic = avi->ic;
    AVFrame *frame = av_frame_alloc();
    AVFrame *outframe = frame;
    AVCodecContext *avctx = NULL;
    AVPacket pkt;
    struct SwsContext *sws = avi->sws;
    int i, err, got_pic = 0;

    if (avi->sws) {
        outframe = av_frame_alloc();
        outframe->format = avi->pixfmt;
        outframe->width = avi->w;
        outframe->height = avi->h;
        av_frame_get_buffer(outframe, 8);
    }

    av_init_packet(&pkt);
    while (!got_pic) {
    err = av_read_frame(ic, &pkt);

    if (err < 0) {
        if (err == AVERROR_EOF || ic->pb && ic->pb->eof_reached) {
            caml_raise_constant(*eof_exception);
        }
        if (ic->pb && ic->pb->error) {
            caml_failwith("pb_error");
        }
    }

    avctx = ic->streams[pkt.stream_index]->codec;
    if (avctx->codec_type == AVMEDIA_TYPE_VIDEO) {
        avcodec_decode_video2(avctx, frame, &got_pic, &pkt);
        if (got_pic && sws) {
            sws_scale(sws, (const uint8_t* const*)frame->data,
                frame->linesize, 0, avi->src_h, outframe->data,
                outframe->linesize);
            av_frame_unref(frame);
        }
    }
    }

    ret = caml_alloc_custom(&avframe_ops, sizeof(AVFrame**), 0, 1);
    memcpy(Data_custom_val(ret), &outframe, sizeof(AVFrame**));

    av_free_packet(&pkt);
    if (outframe != frame) av_frame_free(&frame);

    CAMLreturn(ret);
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

    err = avformat_open_input(&ic, String_val(str),  NULL, NULL);
    if (err < 0)
        caml_failwith("Unable to open input context");
    avformat_find_stream_info(ic, NULL);
    for (i = 0; i < ic->nb_streams; i++) {
        avctx = ic->streams[i]->codec;
        codec = avcodec_find_decoder(avctx->codec_id);
        if (!codec || avcodec_open2(avctx, codec, NULL))
            caml_failwith("Unable to open codec");
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
            caml_raise_constant(*eof_exception);
        }
        if (ic->pb && ic->pb->error) {
            caml_failwith("pb_error");
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

CAMLprim value
libav_init(value unit)
{
    av_register_all();
    eof_exception = (value*)caml_named_value("eof exception");
    return Val_unit;
}
