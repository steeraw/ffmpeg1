#include <stdio.h>
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
#include "jpeglib.h"
#include "SDL2/SDL.h"
#include "SDL2/SDL_thread.h"
#include <time.h>
#include <stdlib.h>


void save_frame_as_jpeg(AVCodecContext *pCodecCtx, AVFrame *pFrame, int FrameNo)
{
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);

    char szFilename[32];

    FILE *outfile;
    sprintf(szFilename, "file%d.jpg", FrameNo);
    if ((outfile = fopen(szFilename, "wb")) == NULL)
    {
        fprintf(stderr, "can't open %s\n", szFilename);
        exit(1);
    }
    jpeg_stdio_dest(&cinfo, outfile);


    cinfo.image_width = pCodecCtx->width;
    cinfo.image_height = pCodecCtx->height;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, 10, TRUE);
//                    image_width             Width of image, in pixels
//                    image_height            Height of image, in pixels
//                    input_components        Number of color channels (samples per pixel)
//                    in_color_space          Color space of source image
    jpeg_start_compress(&cinfo, TRUE);

    JSAMPROW row_pointer[1];        /* pointer to a single row */
    int row_stride;                 /* physical row width in buffer */

    row_stride = cinfo.image_width * 3;   /* JSAMPLEs per row in image_buffer */

    while (cinfo.next_scanline < cinfo.image_height)
    {
        row_pointer[0] = pFrame->data[0]+cinfo.next_scanline * row_stride;// &image_buffer
        jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
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
    AVStream *stream = NULL;


    double arg = strtod(argv[2], NULL);
//    arg /= 100;
    printf("EEEEEEEEE %f\n",arg);


    if(argc < 2)
    {
        fprintf(stderr, "Usage: test <file>\n");
        exit(1);
    }
    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER))
    {
        fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());
        exit(1);
    }


// Open video file
    if(avformat_open_input(&pFormatCtx, argv[1], NULL, NULL)!=0)//argv[1]
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
    av_dump_format(pFormatCtx, 0, argv[1], 0);


//    stream = avformat_new_stream(pFormatCtx, pCodec);
//         if (!stream)
//         {
//             fprintf(stderr, "Could not allocate stream\n");
//             exit(1);
//         }
//    stream->id = pFormatCtx->nb_streams-1;
//    pCodecCtxOrig = stream->codec;

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



// Allocate video frame
    pFrame=av_frame_alloc();
// Allocate an AVFrame structure
    pFrameRGB=av_frame_alloc();
    if(pFrameRGB==NULL)
        return -1;





    SDL_Window *window = SDL_CreateWindow(
            "SDL2Test",
            SDL_WINDOWPOS_UNDEFINED,
            SDL_WINDOWPOS_UNDEFINED,
            pCodecCtx->width,
            pCodecCtx->height,
            0
    );
//    screen = SDL_SetVideoMode(pCodecCtx->width, pCodecCtx->height, 0, 0);
//    if(!screen)
//    {
//        fprintf(stderr, "SDL: could not set video mode - exiting\n");
//        exit(1);
//    }




// Determine required buffer size and allocate buffer
    numBytes=avpicture_get_size(AV_PIX_FMT_YUV420P, pCodecCtx->width,
                                pCodecCtx->height);
    buffer=(uint8_t *)av_malloc(numBytes*sizeof(uint8_t));



    // Assign appropriate parts of buffer to image planes in pFrameRGB
// Note that pFrameRGB is an AVFrame, but AVFrame is a superset
// of AVPicture
    avpicture_fill((AVPicture *)pFrameRGB, buffer, AV_PIX_FMT_YUV420P,
                   pCodecCtx->width, pCodecCtx->height);
    ////AV_PIX_FMT_YUV420P  AV_PIX_FMT_RGB24




//    SDL_Overlay     *bmp = NULL;
//
//    bmp = SDL_CreateYUVOverlay(pCodecCtx->width, pCodecCtx->height,
//                               SDL_YV12_OVERLAY, screen);
    SDL_Event event;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    Uint8 *yPlane, *uPlane, *vPlane;
    size_t yPlaneSz, uvPlaneSz;
    int uvPitch;










    renderer = SDL_CreateRenderer(window, -1, 0);
    if (!renderer) {
        fprintf(stderr, "SDL: could not create renderer - exiting\n");
        exit(1);
    }



    texture = SDL_CreateTexture(
            renderer,
            SDL_PIXELFORMAT_YV12,
            SDL_TEXTUREACCESS_STREAMING,
            pCodecCtx->width,
            pCodecCtx->height
    );
    if (!texture) {
        fprintf(stderr, "SDL: could not create texture - exiting\n");
        exit(1);
    }


    // initialize SWS context for software scaling
    sws_ctx = sws_getContext(pCodecCtx->width,
                             pCodecCtx->height,
                             pCodecCtx->pix_fmt,
                             pCodecCtx->width,
                             pCodecCtx->height,
                             AV_PIX_FMT_YUV420P,
                             SWS_BILINEAR,
                             NULL,
                             NULL,
                             NULL
    ); ///SWS_BILINEAR


    // set up YV12 pixel array (12 bits per pixel)
    yPlaneSz = pCodecCtx->width * pCodecCtx->height;
    uvPlaneSz = pCodecCtx->width * pCodecCtx->height / 4;
    yPlane = (Uint8*)malloc(yPlaneSz);
    uPlane = (Uint8*)malloc(uvPlaneSz);
    vPlane = (Uint8*)malloc(uvPlaneSz);
    if (!yPlane || !uPlane || !vPlane) {
        fprintf(stderr, "Could not allocate pixel buffers - exiting\n");
        exit(1);
    }

    uvPitch = pCodecCtx->width / 2;



    double fps = 0;
    fps = (double)pFormatCtx->streams[videoStream]->r_frame_rate.num / (double)pFormatCtx->streams[videoStream]->r_frame_rate.den;
//    fps = pCodecCtx->framerate.num / pCodecCtx->framerate.den;
//    frame.pts = av_rescale_q(packet.pts, packetTimeBase, frameTimeBase);
//    pFrame->pts = av_rescale_q(packet.pts, pFormatCtx->streams[videoStream]->time_base, pCodecCtx->time_base);
//    printf("%ld", pFrame->pts);
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    i=0;


    static int PTS = 0;
    static int PTS2 = 0;
    long double msec = 0;
    struct timespec time1, time2;



    AVRational a = {1, 1000};
    clock_gettime(CLOCK_REALTIME, &time2);
    while(av_read_frame(pFormatCtx, &packet)>=0)
    {
        // Is this a packet from the video stream?
        if(packet.stream_index==videoStream)
        {
            ///pts = 0;
            // Decode video frame
            avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);









            // Did we get a video frame?
            if(frameFinished)
            {
//                 Convert the image from its native format to RGB
//                sws_scale(sws_ctx, (uint8_t const * const *)pFrame->data, pFrame->linesize, 0, pCodecCtx->height, pFrameRGB->data, pFrameRGB->linesize);

                pFrame->pts = av_rescale_q(packet.pts, pFormatCtx->streams[videoStream]->time_base, a);
//            pFrame->pts = packet.pts / pFormatCtx->streams[videoStream]->time_base * pCodecCtx->time_base;
                PTS = pFrame->pts;


//                msec = 1000000*(time1.tv_sec - time2.tv_sec)+(time1.tv_nsec - time2.tv_nsec);
//            printf("%ld\n",time1.tv_nsec/100000);



                while (TRUE)
                {
                    clock_gettime(CLOCK_REALTIME, &time1);
                    msec = 1000*(time1.tv_sec - time2.tv_sec) + (time1.tv_nsec - time2.tv_nsec)/1000000;
//            printf("%ld\n",time1.tv_nsec/100000);
                    if (PTS <= msec * arg)
                        break;
                }
                    printf("Time from start video = %Lf\n", msec);

                //time1 = time2;









                AVPicture pict;
                pict.data[0] = yPlane;
                pict.data[1] = uPlane;
                pict.data[2] = vPlane;
                pict.linesize[0] = pCodecCtx->width;
                pict.linesize[1] = uvPitch;
                pict.linesize[2] = uvPitch;

                // Convert the image into YUV format that SDL uses
                sws_scale(sws_ctx, (uint8_t const * const *) pFrame->data, pFrame->linesize, 0, pCodecCtx->height, pict.data, pict.linesize);

                SDL_UpdateYUVTexture(
                        texture,
                        NULL,
                        yPlane,
                        pCodecCtx->width,
                        uPlane,
                        uvPitch,
                        vPlane,
                        uvPitch
                );

                SDL_RenderClear(renderer);
                SDL_RenderCopy(renderer, texture, NULL, NULL);
                SDL_RenderPresent(renderer);

                //clock_gettime(CLOCK_REALTIME, &time2);


//                SDL_Delay(PTS - PTS2);



                PTS2 = PTS;
                printf("Current PTS = %d\n",PTS);
//                SDL_Delay(fps);
//                printf("%f ",fps);
//                SDL_Delay(1000 / fps);
//                printf("%d ",pCodecCtx->frame_number);
//                // Save the frame to disk
//                if(++i%25==0)
//                {
//
////                    SaveFrame(pFrameRGB, pCodecCtx->width, pCodecCtx->height, i);
////                    save_frame_as_jpeg(pCodecCtx, pFrameRGB, i);
//                }


            }
        }

        // Free the packet that was allocated by av_read_frame
        //av_free_packet(&packet);////DEPRECATED
        av_packet_unref(&packet);
        SDL_PollEvent(&event);
        switch (event.type)
        {
        case SDL_QUIT:
            SDL_DestroyTexture(texture);
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            SDL_Quit();
                break;
        default:
            break;
        }
    }

//// Free the YUV frame
av_frame_free(&pFrame);
free(yPlane);
free(uPlane);
free(vPlane);

//// Free the RGB image
    av_free(buffer);
//    buffer = NULL;

    av_frame_free(&pFrameRGB);
//
//// Free the YUV frame
    av_free(pFrame);
//// Close the codecs
//    avcodec_close(pCodecCtx);
//      avcodec_free_context(&pCodecCtxOrig);
    avcodec_close(pCodecCtxOrig);
    avcodec_close(pCodecCtx);
    // Close the video file
    avformat_close_input(&pFormatCtx);

    return 0;
}