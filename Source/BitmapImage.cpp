#include "BitmapImage.h"

void *BI_def_bitmap_create(int width, int height)          {return Allocate(width * height * 4);}
void  BI_def_bitmap_set_opaque(void *bitmap, BOOL opaque)  {}
BOOL  BI_def_bitmap_test_opaque(void *bitmap)              {return false;}
unsigned char *BI_def_bitmap_get_buffer(void *bitmap)      {return (unsigned char*)bitmap;}
void BI_def_bitmap_destroy(void *bitmap)                   {Free(bitmap);}
void BI_def_bitmap_modified(void *bitmap)                  {}


BitmapImage::BitmapImage()
{
    bitmap_callbacks.bitmap_create = BI_def_bitmap_create;
    bitmap_callbacks.bitmap_destroy = BI_def_bitmap_destroy;
    bitmap_callbacks.bitmap_get_buffer = BI_def_bitmap_get_buffer;
    bitmap_callbacks.bitmap_modified = BI_def_bitmap_modified;
    bitmap_callbacks.bitmap_set_opaque = BI_def_bitmap_set_opaque;
    bitmap_callbacks.bitmap_test_opaque = BI_def_bitmap_test_opaque;
}

BitmapImage::~BitmapImage()
{
    if(bIsAnimatedGif)
    {
        gif_finalise(&gif);
        Free(animationFrameCache);
        Free(animationFrameData);
    }

    if(lpGifData)
        Free(lpGifData);

    EnableFileMonitor(false);

    delete texture;
}

//----------------------------------------------------------------------------

void BitmapImage::CreateErrorTexture(void)
{
    LPBYTE textureData = (LPBYTE)Allocate(32*32*4);
    msetd(textureData, 0xFF0000FF, 32*32*4);

    texture = CreateTexture(32, 32, GS_RGB, textureData, FALSE);
    fullSize.Set(32.0f, 32.0f);

    Free(textureData);
}


//----------------------------------------------------------------------------

void BitmapImage::SetPath(String path)
{
    filePath = path;
}

void BitmapImage::EnableFileMonitor(bool bMonitor)
{
    if (changeMonitor)
    {
        OSMonitorFileDestroy(changeMonitor);
        changeMonitor = NULL;
    }

    if (bMonitor)
        changeMonitor = OSMonitorFileStart(filePath);
}

void BitmapImage::Init(void)
{
    if(bIsAnimatedGif)
    {
        bIsAnimatedGif = false;
        gif_finalise(&gif);

        Free(animationFrameCache);
        animationFrameCache = NULL;
        Free(animationFrameData);
        animationFrameData = NULL;
    }

    if(lpGifData)
    {
        Free(lpGifData);
        lpGifData = NULL;
    }

    animationTimes.Clear();

    delete texture;
    texture = NULL;

    CTSTR lpBitmap = filePath;
    if(!lpBitmap || !*lpBitmap)
    {
        AppWarning(TEXT("BitmapImage::Init: Empty path"));
        CreateErrorTexture();
        return;
    }

    //------------------------------------

    if(GetPathExtension(lpBitmap).CompareI(TEXT("gif")))
    {
        gif_create(&gif, &bitmap_callbacks);

        XFile gifFile;
        if(!gifFile.Open(lpBitmap, XFILE_READ, XFILE_OPENEXISTING))
        {
            AppWarning(TEXT("BitmapImage::Init: could not open gif file '%s'"), lpBitmap);
            CreateErrorTexture();
            return;
        }


        DWORD fileSize = (DWORD)gifFile.GetFileSize();
        lpGifData = (LPBYTE)Allocate(fileSize);
        gifFile.Read(lpGifData, fileSize);

        gif_result result;
        do
        {
            result = gif_initialise(&gif, fileSize, lpGifData);
            if(result != GIF_OK && result != GIF_WORKING)
            {
                Log(TEXT("BitmapImage: Warning, couldn't initialise gif %s, it is likely corrupt"), lpBitmap);
                CreateErrorTexture();
                return;
            }
        }while(result != GIF_OK);

        if (gif.width > 4096 || gif.height > 4096)
        {
            Log(TEXT("BitmapImage: Warning, bad texture dimensions %d x %d in %s"), gif.width, gif.height, lpBitmap);
            CreateErrorTexture();
            return;
        }

        unsigned long long max_size = (unsigned long long)gif.width * (unsigned long long)gif.height * 4LLU * (unsigned long long)gif.frame_count;
        if (gif.width * gif.height * 4 * gif.frame_count != max_size)
        {
            Log(TEXT("BitmapImage: Warning, gif %s overflowed maximum pointer size and was not loaded (%llu > %u)"), lpBitmap, max_size, gif.width * gif.height * 4 * gif.frame_count);
            CreateErrorTexture();
            return;
        }

        if(gif.frame_count > 1)
        {
            if(result == GIF_OK || result == GIF_WORKING)
                bIsAnimatedGif = true;
        }

        if(bIsAnimatedGif)
        {
            gif_decode_frame(&gif, 0);
            texture = CreateTexture(gif.width, gif.height, GS_RGBA, gif.frame_image, FALSE, FALSE);

            animationFrameCache = (BYTE **)Allocate(gif.frame_count * sizeof(BYTE *));
            memset(animationFrameCache, 0, gif.frame_count * sizeof(BYTE *));

            animationFrameData = (BYTE *)Allocate(gif.frame_count * gif.width * gif.height * 4);
            memset(animationFrameData, 0, gif.frame_count * gif.width * gif.height * 4);

            for(UINT i=0; i<gif.frame_count; i++)
            {
                float frameTime = float(gif.frames[i].frame_delay)*0.01f;
                if (frameTime == 0.0f)
                    frameTime = 0.1f;
                animationTimes << frameTime;

                if (gif_decode_frame(&gif, i) != GIF_OK)
                    Log (TEXT("BitmapImage: Warning, couldn't decode frame %d of %s"), i, lpBitmap);
            }

            gif_decode_frame(&gif, 0);

            fullSize.x = float(gif.width);
            fullSize.y = float(gif.height);

            curTime = 0.0f;
            curFrame = 0;
            lastDecodedFrame = 0;
        }
        else
        {
            gif_finalise(&gif);
            Free(lpGifData);
            lpGifData = NULL;
        }
    }

    if(!bIsAnimatedGif)
    {
        texture = GS->CreateTextureFromFile(lpBitmap, TRUE);
        if(!texture)
        {
            AppWarning(TEXT("BitmapImage::Init: could not create texture '%s'"), lpBitmap);
            CreateErrorTexture();
            return;
        }

        fullSize.x = float(texture->Width());
        fullSize.y = float(texture->Height());
    }

}

Vect2 BitmapImage::GetSize(void) const
{
    return fullSize;
}

Texture* BitmapImage::GetTexture(void) const
{
    return texture;
}

void BitmapImage::Tick(float fSeconds)
{
    if(bIsAnimatedGif)
    {
        UINT totalLoops = (UINT)gif.loop_count;
        if(totalLoops >= 0xFFFF)
            totalLoops = 0;

        if(!totalLoops || curLoop < totalLoops)
        {
            UINT newFrame = curFrame;

            curTime += fSeconds;
            while(curTime > animationTimes[newFrame])
            {
                curTime -= animationTimes[newFrame];
                if(++newFrame == animationTimes.Num())
                {
                    if(!totalLoops || ++curLoop < totalLoops)
                        newFrame = 0;
                    else if (curLoop == totalLoops)
                    {
                        newFrame--;
                        break;
                    }
                }
            }

            if(newFrame != curFrame)
            {
                UINT lastFrame;

                if (!animationFrameCache[newFrame])
                {
                    //animation might have looped, if so make sure we decode from frame 0
                    if (newFrame < lastDecodedFrame)
                        lastFrame = 0;
                    else
                        lastFrame = lastDecodedFrame + 1;

                    //we need to decode any frames we missed for consistency
                    for (UINT i = lastFrame; i < newFrame; i++)
                    {
                        if (gif_decode_frame(&gif, i) != GIF_OK)
                            return;
                    }

                    //now decode and display the actual frame we want
                    int ret = gif_decode_frame(&gif, newFrame);
                    if (ret == GIF_OK)
                    {
                        animationFrameCache[newFrame] = animationFrameData + (newFrame * (gif.width * gif.height * 4));
                        memcpy(animationFrameCache[newFrame], gif.frame_image, gif.width * gif.height * 4);
                    }

                    lastDecodedFrame = newFrame;
                }

                if (animationFrameCache[newFrame])
                    texture->SetImage(animationFrameCache[newFrame], GS_IMAGEFORMAT_RGBA, gif.width*4);

                curFrame = newFrame;
            }
        }
    }

    if (updateImageTime)
    {
        updateImageTime -= fSeconds;
        if (updateImageTime <= 0.0f)
        {
            updateImageTime = 0.0f;
            Init();
        }
    }

    if (changeMonitor && OSFileHasChanged(changeMonitor))
        updateImageTime = 1.0f;
}
