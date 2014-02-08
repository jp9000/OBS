#pragma once

#include "Main.h"
#include "libnsgif.h"


class BitmapImage{
    Texture *texture;
    Vect2 fullSize;

    bool bIsAnimatedGif;
    gif_animation gif;
    LPBYTE lpGifData;
    List<float> animationTimes;
    BYTE **animationFrameCache;
    BYTE *animationFrameData;
    UINT curFrame, curLoop, lastDecodedFrame;
    float curTime;
    float updateImageTime;

    String filePath;
    OSFileChangeData *changeMonitor;

    gif_bitmap_callback_vt bitmap_callbacks;

    void CreateErrorTexture(void);

public:
    BitmapImage();
    ~BitmapImage();

    void SetPath(String path);
    void EnableFileMonitor(bool bMonitor);
    void Init(void);

    Vect2 GetSize(void) const;
    Texture* GetTexture(void) const;

    void Tick(float fSeconds);
};
