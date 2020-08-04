#include <stdio.h>
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"

// compatibility with newer API
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(55,28,1)
#define av_frame_alloc avcodec_alloc_frame
#define av_frame_free avcodec_free_frame
#endif


void save_frame_as_jpeg(AVCodecContext *pCodecCtx, AVFrame *pFrame, int FrameNo)
{
    AVCodec *jpegCodec = avcodec_find_encoder(AV_CODEC_ID_JPEG2000); //AV_CODEC_ID_MJPEG ////AV_CODEC_ID_PNG?
    if (!jpegCodec)
    {
        return;
    }
    AVCodecContext *jpegContext = avcodec_alloc_context3(jpegCodec);
    if (!jpegContext)
    {
        return;
    }

    jpegContext->pix_fmt = pCodecCtx->pix_fmt;  //
    jpegContext->height = pFrame->height;
    jpegContext->width = pFrame->width;

//    pOCodecCtx->bit_rate = pCodecCtxOrig->bit_rate;
//    pOCodecCtx->width = pCodecCtxOrig->width;
//    pOCodecCtx->height = pCodecCtxOrig->height;
//    pOCodecCtx->pix_fmt = AV_PIX_FMT_YUVJ420P;
//    pOCodecCtx->codec_id = AV_CODEC_ID_MJPEG;
//    pOCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
//    pOCodecCtx->time_base.num = pCodecCtxOrig->time_base.num;
//    pOCodecCtx->time_base.den = pCodecCtxOrig->time_base.den;
//    jpegContext->time_base.num = pCodecCtx->time_base.num;
//    jpegContext->time_base.den = pCodecCtx->time_base.den;
    jpegContext->time_base = (AVRational){1,25};


    if (avcodec_open2(jpegContext, jpegCodec, NULL) < 0)
    {
        return;
    }
    FILE *JPEGFile;
    char JPEGFName[256];

    AVPacket packet = {.data = NULL, .size = 0};
    av_init_packet(&packet);
    int gotFrame;

    if (avcodec_encode_video2(jpegContext, &packet, pFrame, &gotFrame) < 0)
    {
        printf("Error encoding frame\n");
        return;
    }

    sprintf(JPEGFName, "dvr-%d.jpg", FrameNo);
    JPEGFile = fopen(JPEGFName, "wb");
    fwrite(packet.data, 1, packet.size, JPEGFile);
    fclose(JPEGFile);

    av_free_packet(&packet);
    avcodec_close(jpegContext);
}

void SaveFrame(AVFrame *pFrame, int width, int height, int iFrame)
{
    FILE *pFile;
    char szFilename[32];
    int  y;

    // Open file
    sprintf(szFilename, "frame%d.ppm", iFrame);
    pFile=fopen(szFilename, "wb");
    if(pFile==NULL)
    {
        printf("pFile == NULL");
        return;
    }


    // Write header
    fprintf(pFile, "P6\n%d %d\n255\n", width, height);

    // Write pixel data
    for(y=0; y<height; y++)
        fwrite(pFrame->data[0]+y*pFrame->linesize[0], 1, width*3, pFile);

    // Close file
    fclose(pFile);
}



int main(int argc, char *argv[])
{
    av_register_all();

    AVFormatContext *pFormatCtx = NULL;
    int i,videoStream,numBytes;
    AVCodecContext *pCodecCtxOrig = NULL;
    AVCodecContext *pCodecCtx = NULL;
    AVCodec *pCodec = NULL;
    AVCodecParameters *pParam = avcodec_parameters_alloc();
    AVFrame *pFrame;
    AVFrame *pFrameRGB = NULL;
    uint8_t *buffer = NULL;
    struct SwsContext *sws_ctx = NULL;
    int frameFinished;
    AVPacket packet;


    AVCodec *pOCodec = NULL;
    AVCodecContext *pOCodecCtx = NULL;

// Open video file
    if(avformat_open_input(&pFormatCtx, "videoplayback", NULL, NULL)!=0)//argv[1]
    {
        printf("Couldn't open file\n");
        return -1; // Couldn't open file
    }


    // Retrieve stream information
    if(avformat_find_stream_info(pFormatCtx, NULL)<0)
    {
        printf("Couldn't find stream information\n");
        return -1; // Couldn't find stream information
    }

    // Dump information about file onto standard error
    av_dump_format(pFormatCtx, 0, "videoplayback", 0);



// Find the first video stream
    videoStream=-1;
    for(i=0; i<pFormatCtx->nb_streams; i++)
        if(pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO)
        {
            videoStream=i;
            break;
        }
    if(videoStream==-1)
        return -1; // Didn't find a video stream

// Get a pointer to the codec context for the video stream
    pCodecCtxOrig = pFormatCtx->streams[videoStream]->codec;


// Find the decoder for the video stream
    pCodec=avcodec_find_decoder(pCodecCtxOrig->codec_id);
    if(pCodec==NULL)
    {
        fprintf(stderr, "Unsupported codec!\n");
        return -1; // Codec not found
    }
// Copy context
    pCodecCtx = avcodec_alloc_context3(pCodec);
            ////FIXED
    ////avcodec_copy_context(pCodecCtx, pCodecCtxOrig);
//    if(avcodec_copy_context(pCodecCtx, pCodecCtxOrig) != 0)
//    {
//        fprintf(stderr, "Couldn't copy codec context");
//        return -1; // Error copying codec context
//    }
            ////FIXED
    avcodec_parameters_from_context(pParam, pCodecCtxOrig);
    avcodec_parameters_to_context(pCodecCtx, pParam);
    avcodec_parameters_free(&pParam);

//// Open codec
    if(avcodec_open2(pCodecCtx, pCodec, NULL)<0)
        return -1; // Could not open codec
//    pOCodecCtx = avcodec_alloc_context3(pOCodec);
//    if (!pOCodecCtx) {
//        fprintf(stderr, "Could not allocate codec\n");
//        return 1;
//    }


// Allocate video frame
    pFrame=av_frame_alloc();
// Allocate an AVFrame structure
    pFrameRGB=av_frame_alloc();
    if(pFrameRGB==NULL)
        return -1;


// Determine required buffer size and allocate buffer
    numBytes=avpicture_get_size(AV_PIX_FMT_RGB24, pCodecCtx->width,
                                pCodecCtx->height);
    buffer=(uint8_t *)av_malloc(numBytes*sizeof(uint8_t));



    // Assign appropriate parts of buffer to image planes in pFrameRGB
// Note that pFrameRGB is an AVFrame, but AVFrame is a superset
// of AVPicture
    avpicture_fill((AVPicture *)pFrameRGB, buffer, AV_PIX_FMT_RGB24,
                   pCodecCtx->width, pCodecCtx->height);
    ////AV_PIX_FMT_YUVJ420P  AV_PIX_FMT_RGB24

//    ////
//
//
//
//
//
//
//
//    pOCodecCtx->bit_rate = pCodecCtxOrig->bit_rate;
//    pOCodecCtx->width = pCodecCtxOrig->width;
//    pOCodecCtx->height = pCodecCtxOrig->height;
//    pOCodecCtx->pix_fmt = AV_PIX_FMT_YUVJ420P;
//    pOCodecCtx->codec_id = AV_CODEC_ID_MJPEG;
//    pOCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
//    pOCodecCtx->time_base.num = pCodecCtxOrig->time_base.num;
//    pOCodecCtx->time_base.den = pCodecCtxOrig->time_base.den;
//
//    pOCodec = avcodec_find_encoder(pOCodecCtx->codec_id);
//    if(!pOCodec)
//    {
//        fprintf(stderr, "Codec not found\n");
//        return 1;
//
//    }
//    else
//        fprintf(stderr, "Codec with id CODEC_ID_MJPEG found\n");
//
//    if (avcodec_open2(pOCodecCtx, pOCodec, NULL) < 0) {
//        fprintf(stderr, "Could not open codec\n");
//        return 1;
//    }
//    else
//        fprintf(stderr, "Codec was opened\n");
//
//
//    ////
    // initialize SWS context for software scaling
    sws_ctx = sws_getContext(pCodecCtx->width,
                             pCodecCtx->height,
                             pCodecCtx->pix_fmt,
                             pCodecCtx->width,
                             pCodecCtx->height,
                             AV_PIX_FMT_RGB24,
                             SWS_BILINEAR,
                             NULL,
                             NULL,
                             NULL
    );


    ////


//    while(av_read_frame(pFormatCtx, &packet)>=0)
//    {
//        // Is this a packet from the video stream?
//        if(packet.stream_index==videoStream)
//        {
//            // Decode video frame
//            avcodec_decode_video2(pOCodecCtx, pFrame, &frameFinished,
//                                 &packet);
//
//            // Did we get a video frame?
//            if(frameFinished)
//            {
//                printf("EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE");
//
////                pCodecCtx->qmin = pCodecCtx->qmax = 3;
////                pCodecCtx->mb_lmin = pCodecCtx->qmin * FF_QP2LAMBDA;
////                pCodecCtx->mb_lmax = pCodecCtx->qmax * FF_QP2LAMBDA;
////                pCodecCtx->flags |= AV_CODEC_FLAG_QSCALE;
////
////
////
////
////
////                pFrame->quality = 4;
////                pFrame->pts = i;
////
////
////
////                int szBufferActual = avcodec_encode_video2(pCodecCtx, buffer, numBytes, pFrame);
////                if(szBufferActual < 0)
////                {
////                    fprintf(stderr, "avcodec_encode_video error. return value = %d\n",szBufferActual);
////                    return -1;
////                }
////
////                if(++i%25==0) //++i < atoi(argv[2])
////                {
////                    /* Write JPEG to file */
////                    char buf[32] = {0};
////                    snprintf(buf,30,"fram%d.jpeg",i);
////                    FILE *fdJPEG = fopen(buf, "wb");
////                    int bRet = fwrite(buffer, sizeof(uint8_t), szBufferActual, fdJPEG);
////                    fclose(fdJPEG);
////
////                    if (bRet != szBufferActual)
////                    {
////                        fprintf(stderr, "Error writing jpeg file\n");
////                        return 1;
////                    }
////                    else
////                        fprintf(stderr, "jpeg file was writed\n");
////                }
////                //else
////                    //break;
//
//            }
//        }
//
//        // Free the packet that was allocated by av_read_frame
//        av_free_packet(&packet);
//    }


    ////
    i=0;
    while(av_read_frame(pFormatCtx, &packet)>=0)
    {
        // Is this a packet from the video stream?
        if(packet.stream_index==videoStream)
        {
            // Decode video frame
            avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);

            // Did we get a video frame?
            if(frameFinished)
            {
                // Convert the image from its native format to RGB
                sws_scale(sws_ctx, (uint8_t const * const *)pFrame->data,
                          pFrame->linesize, 0, pCodecCtx->height,
                          pFrameRGB->data, pFrameRGB->linesize);

                // Save the frame to disk
                if(++i%25==0)
                {
//                    SaveFrame(pFrameRGB, pCodecCtx->width,
//                              pCodecCtx->height, i);
                    save_frame_as_jpeg(pCodecCtx, pFrame, i);
                }


            }
        }

        // Free the packet that was allocated by av_read_frame
//        av_free_packet(&packet);
    }






//// Free the RGB image
//    av_free(buffer);
//    buffer = NULL;

    av_frame_free(&pFrameRGB);
//
//// Free the YUV frame
    av_free(pFrame);


//    if (avcodec_is_open(pCodecCtx))
//        printf("avcodecCTX_is_open\n");
//    if (avcodec_is_open(pCodecCtxOrig))
//        printf("avcodecORIG_is_open\n");
//// Close the codecs
//    avcodec_close(pCodecCtx);
//      avcodec_free_context(&pCodecCtxOrig);
    avcodec_close(pCodecCtxOrig);

    // Close the video file
    avformat_close_input(&pFormatCtx);




    return 0;
}