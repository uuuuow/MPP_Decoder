#define LOG_TAG "H264Decoder"
#include <android/log.h>
#include <jni.h>
#include <unistd.h>
#include <stdio.h>
#include <rk_mpi.h>
#include <malloc.h>

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

void dump_frame(MppFrame frame, FILE *out_fp) {
    LOGD("dump_frame_to_file");
    RK_U32 width    = 0;
    RK_U32 height   = 0;
    RK_U32 h_stride = 0;
    RK_U32 v_stride = 0;
    MppFrameFormat fmt  = MPP_FMT_YUV420SP;
    MppBuffer buffer    = NULL;
    RK_U8 *base = NULL;

    width    = mpp_frame_get_width(frame);
    height   = mpp_frame_get_height(frame);
    h_stride = mpp_frame_get_hor_stride(frame);
    v_stride = mpp_frame_get_ver_stride(frame);
    fmt      = mpp_frame_get_fmt(frame);
    buffer   = mpp_frame_get_buffer(frame);

    RK_U32 buf_size = mpp_frame_get_buf_size(frame);
    LOGD("w x h: %dx%d hor_stride:%d ver_stride:%d buf_size:%d\n",
           width, height, h_stride, v_stride, buf_size);

    if (NULL == buffer) {
        printf("buffer is null\n");
        return ;
    }

    base = (RK_U8 *)mpp_buffer_get_ptr(buffer);

    if (fmt != MPP_FMT_YUV420SP) {
        printf("fmt %d not supported\n", fmt);
        return;
    }

    RK_U32 i;
    RK_U8 *base_y = base;
    RK_U8 *base_c = base + h_stride * v_stride;

    for (i = 0; i < height; i++, base_y += h_stride) {
        fwrite(base_y, 1, width, out_fp);
    }
    for (i = 0; i < height / 2; i++, base_c += h_stride) {
        fwrite(base_c, 1, width, out_fp);
    }
}

void dump_frame_to_file(MppCtx ctx, MppApi *mpi, MppFrame frame, FILE *out_fp)
{
    LOGD("decode_and_dump_to_file\n");

    MPP_RET ret;

    if (mpp_frame_get_info_change(frame)) {
        printf("mpp_frame_get_info_change\n");
        ret = mpi->control(ctx, MPP_DEC_SET_INFO_CHANGE_READY, NULL);
        if (ret) {
            LOGD("mpp_frame_get_info_change mpi->control error"
                 "MPP_DEC_SET_INFO_CHANGE_READY %d\n", ret);
        }
        return;
    }
    RK_U32 err_info = mpp_frame_get_errinfo(frame);
    RK_U32 discard = mpp_frame_get_discard(frame);
    printf("err_info: %u discard: %u\n", err_info, discard);

    if (err_info) {
        return;
    }

    dump_frame(frame, out_fp);
    return;
}

extern "C" JNIEXPORT jint JNICALL
Java_com_example_h264decoder_MainActivity_decode(JNIEnv *env, jobject obj, jstring jInputPath, jstring jOutputPath) {
    const char *inputPath = (env)->GetStringUTFChars(jInputPath, nullptr);
    const char *outputPath = (env)->GetStringUTFChars( jOutputPath, nullptr);

    LOGD("Input file: %s", inputPath);
    LOGD("Output file: %s", outputPath);

    FILE *in_fp = fopen(inputPath, "rb");
    if (!in_fp) {
        LOGE("Failed to open input file");
        return -1;
    }

    FILE *out_fp = fopen(outputPath, "wb");
    if (!out_fp) {
        LOGE("Failed to open output file");
        fclose(in_fp);
        return -1;
    }

    MppCtx ctx = NULL;
    MppApi *mpi = NULL;
    MPP_RET ret = mpp_create(&ctx, &mpi);
    if (MPP_OK != ret) {
        LOGE("mpp_create failed");
        fclose(in_fp);
        fclose(out_fp);
        return -1;
    }

    RK_U32 need_split = -1;
    if (MPP_OK != mpi->control(ctx, MPP_DEC_SET_PARSER_SPLIT_MODE, (MppParam*)&need_split)) {
        LOGE("MPP_DEC_SET_PARSER_SPLIT_MODE failed");
        fclose(in_fp);
        fclose(out_fp);
        return -1;
    }

    if (MPP_OK != mpp_init(ctx, MPP_CTX_DEC, MPP_VIDEO_CodingAVC)) {
        LOGE("mpp_init failed");
        fclose(in_fp);
        fclose(out_fp);
        return -1;
    }

    int buf_size = 5 * 1024 * 1024;
    char *buf = (char*)malloc(buf_size);
    if (!buf) {
        LOGE("malloc failed");
        fclose(in_fp);
        fclose(out_fp);
        return -1;
    }

    MppPacket pkt = NULL;
    ret = mpp_packet_init(&pkt, buf, buf_size);
    if (MPP_OK != ret) {
        LOGE("mpp_packet_init error\n");
        return -1;
    }

    int over = 0;
    while (!over) {
        int len = fread(buf, 1, buf_size, in_fp);
        if (len > 0) {
            mpp_packet_write(pkt, 0, buf, len);
            mpp_packet_set_pos(pkt, buf);
            mpp_packet_set_length(pkt, len);
            if (feof(in_fp) || len < buf_size) {
                mpp_packet_set_eos(pkt);
            }
        }

        int pkt_is_send = 0;
        while (!pkt_is_send && !over) {
            if (0 < len) {
                LOGD("pkt remain:%d\n", mpp_packet_get_length(pkt));
                ret = mpi->decode_put_packet(ctx, pkt);
                if (MPP_OK == ret) {
                    LOGD("pkt send success remain:%d\n", mpp_packet_get_length(pkt));
                    pkt_is_send = 1;
                }
            }

            MppFrame frame;
            MPP_RET ret;
            ret = mpi->decode_get_frame(ctx, &frame);
            if (MPP_OK != ret || !frame) {
                LOGE("decode_get_frame falied ret:%d\n", ret);
                usleep(2000);  // 等待一下2ms，通常1080p解码时间2ms
                continue;
            }

            LOGD("decode_get_frame success\n");
            dump_frame_to_file(ctx, mpi, frame, out_fp);

            if (mpp_frame_get_eos(frame)) {
                printf("mpp_frame_get_eos\n");
                mpp_frame_deinit(&frame);
                over = 1;
                continue;
            }
            mpp_frame_deinit(&frame);
        }
    }

    fclose(in_fp);
    fclose(out_fp);
    mpi->reset(ctx);
    mpp_packet_deinit(&pkt);
    mpp_destroy(ctx);
    free(buf);

    (env)->ReleaseStringUTFChars( jInputPath, inputPath);
    (env)->ReleaseStringUTFChars( jOutputPath, outputPath);

    LOGD("Decode complete");
    return 0;
}
