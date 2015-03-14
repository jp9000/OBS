#include "stdafx.h"
#include "ObsVCE.h"

using namespace std;
#define CHECKCONFIG(x,y) \
if (x != y)\
    VCELog(TEXT("%s is not set: %d"), TEXT(#x), x);

void quickSet(map<string, int32_t> &configTable, int qs)
{
    //map<string, int32_t> configTable = (map<string, int32_t>)*pConfigTable;
    prepareConfigMap(configTable, true); //reset
    //Shared settings
    configTable["encEnImeOverwDisSubm"] = 0;
    configTable["encImeOverwDisSubmNo"] = 0;
    configTable["forceZeroPointCenter"] = 0;
    configTable["enableAMD"] = 0;
    configTable["encForce16x16skip"] = 0;

    switch (qs)
    {
    case 0: //speed
        configTable["encSearchRangeX"] = 16;
        configTable["encSearchRangeY"] = 16;
        configTable["encDisableSubMode"] = 254;
        configTable["encForce16x16skip"] = 1;
        break;
    case 1: //balanced
        configTable["encSearchRangeX"] = 24;//balanced.cfg has it on 16
        configTable["encSearchRangeY"] = 24;
        configTable["encDisableSubMode"] = 120;
        configTable["encEnImeOverwDisSubm"] = 1;
        configTable["encImeOverwDisSubmNo"] = 1;
        break;
    case 2: //quality
        configTable["forceZeroPointCenter"] = 1;
        configTable["encSearchRangeX"] = 36;
        configTable["encSearchRangeY"] = 36;
        configTable["enableAMD"] = 1;
        configTable["encDisableSubMode"] = 0;
        break;
    default:
        break;
    }
}

//TODO Merge with encodeSetParam? Nice for use with config files but kinda useless here.
void prepareConfigMap(map<string, int32_t> &configTable, bool quickset)
{
    //map<string, int32_t> configTable = (map<string, int32_t>)*pConfigTable;
    /**************************************************************************/
    /* ConfigPicCtl                                                           */
    /**************************************************************************/
    configTable["useConstrainedIntraPred"] = 0;
    configTable["CABACEnable"] = 1;
    configTable["CABACIDC"] = 0;
    configTable["loopFilterDisable"] = 0;
    configTable["encLFBetaOffset"] = 0;
    configTable["encLFAlphaC0Offset"] = 0;
    configTable["encIDRPeriod"] = 0; //IDR every n frames (nice for streaming)
    configTable["encIPicPeriod"] = 0; //I every n frames (sucks for streaming?)
    configTable["encHeaderInsertionSpacing"] = 0;
    configTable["encCropLeftOffset"] = 0;
    configTable["encCropRightOffset"] = 0;
    configTable["encCropTopOffset"] = 0;
    configTable["encCropBottomOffset"] = 0;
    configTable["encNumMBsPerSlice"] = 99;
    configTable["encNumSlicesPerFrame"] = 1;
    configTable["encForceIntraRefresh"] = 0;
    configTable["encForceIMBPeriod"] = 0;
    configTable["encInsertVUIParam"] = 0;
    configTable["encInsertSEIMsg"] = 1<<0 | 1<<1 | 1<<2; //TODO OBS needs SEI?

    /**************************************************************************/
    /* ConfigMotionEstimation                                                 */
    /**************************************************************************/
    configTable["IMEDecimationSearch"] = 1;
    configTable["motionEstHalfPixel"] = 1;
    configTable["motionEstQuarterPixel"] = 1;
    configTable["disableFavorPMVPoint"] = 0;
    configTable["forceZeroPointCenter"] = 0;
    configTable["LSMVert"] = 0;
    configTable["encSearchRangeX"] = 16;
    configTable["encSearchRangeY"] = 16;
    configTable["encSearch1RangeX"] = 0;
    configTable["encSearch1RangeY"] = 0;
    configTable["disable16x16Frame1"] = 0;
    configTable["disableSATD"] = 0;
    configTable["enableAMD"] = 0;
    configTable["encDisableSubMode"] = 254;
    configTable["encIMESkipX"] = 0;
    configTable["encIMESkipY"] = 0;
    configTable["encEnImeOverwDisSubm"] = 0;
    configTable["encImeOverwDisSubmNo"] = 0;
    configTable["encIME2SearchRangeX"] = 4;
    configTable["encIME2SearchRangeY"] = 4;

    /**************************************************************************/
    /* ConfigRDO                                                              */
    /**************************************************************************/
    configTable["encDisableTbePredIFrame"] = 0;
    configTable["encDisableTbePredPFrame"] = 0;
    configTable["useFmeInterpolY"] = 0;
    configTable["useFmeInterpolUV"] = 0;
    configTable["enc16x16CostAdj"] = 0;
    configTable["encSkipCostAdj"] = 0;
    configTable["encForce16x16skip"] = 1;

    if (quickset) return;

    /**************************************************************************/
    /* EncodeSpecifications                                                   */
    /**************************************************************************/
    //configTable["pictureHeight"] = 144;
    //configTable["pictureWidth"] = 176;
    configTable["EncodeMode"] = OVE_AVC_FULL;
    configTable["level"] = 42;
    configTable["profile"] = 77; // 66 -base, 77 - main, 100 - high
    configTable["pictureFormat"] = OVE_PICTURE_FORMAT_NV12;
    configTable["requestedPriority"] = 1;

    /**************************************************************************/
    /* ConfigRateCtl                                                          */
    /**************************************************************************/
    configTable["encRateControlMethod"] = 4;
    configTable["encRateControlTargetBitRate"] = 8000000;
    configTable["encRateControlPeakBitRate"] = 0;
    configTable["encRateControlFrameRateNumerator"] = 30;
    configTable["encRateControlFrameRateDenominator"] = 1;
    configTable["encGOPSize"] = 0;
    configTable["encRCOptions"] = 0;
    configTable["encQP_I"] = 22;
    configTable["encQP_P"] = 22;
    configTable["encQP_B"] = 0; //VCE 2.0 (or 1.1, whatever) on R series supports B?
    //TODO set from OBS?
    configTable["encVBVBufferSize"] = configTable["encRateControlTargetBitRate"] / 2; //4000000;

    //Custom app settings
    configTable["sendFPS"] = 0; //Send proper video fps to encoder. Not sending allows video conversion with weird framerates
}

void encodeSetParam(OvConfigCtrl *pConfig, map<string, int32_t>* pConfigTable)
{
    /**************************************************************************/
    /* fill-in the general configuration structures                           */
    /**************************************************************************/
    map<string, int32_t> configTable = (map<string, int32_t>)*pConfigTable;
    //pConfig->height = configTable["pictureHeight"];
    //pConfig->width  = configTable["pictureWidth"];
    pConfig->encodeMode = (OVE_ENCODE_MODE)configTable["EncodeMode"];

    /**************************************************************************/
    /* fill-in the profile and level                                          */
    /**************************************************************************/
    pConfig->profileLevel.level = configTable["level"];
    pConfig->profileLevel.profile = configTable["profile"];

    pConfig->pictFormat = (OVE_PICTURE_FORMAT)configTable["pictureFormat"];
    pConfig->priority = (OVE_ENCODE_TASK_PRIORITY)configTable["requestedPriority"];

    /**************************************************************************/
    /* fill-in the picture control structures                                 */
    /**************************************************************************/
    pConfig->pictControl.size = sizeof(OVE_CONFIG_PICTURE_CONTROL);
    pConfig->pictControl.useConstrainedIntraPred = configTable["useConstrainedIntraPred"];
    pConfig->pictControl.cabacEnable = configTable["CABACEnable"];
    pConfig->pictControl.cabacIDC = configTable["CABACIDC"];
    pConfig->pictControl.loopFilterDisable = configTable["loopFilterDisable"];
    pConfig->pictControl.encLFBetaOffset = configTable["encLFBetaOffset"];
    pConfig->pictControl.encLFAlphaC0Offset = configTable["encLFAlphaC0Offset"];
    pConfig->pictControl.encIDRPeriod = configTable["encIDRPeriod"];
    pConfig->pictControl.encIPicPeriod = configTable["encIPicPeriod"];
    pConfig->pictControl.encHeaderInsertionSpacing = configTable["encHeaderInsertionSpacing"];
    pConfig->pictControl.encCropLeftOffset = configTable["encCropLeftOffset"];
    pConfig->pictControl.encCropRightOffset = configTable["encCropRightOffset"];
    pConfig->pictControl.encCropTopOffset = configTable["encCropTopOffset"];
    pConfig->pictControl.encCropBottomOffset = configTable["encCropBottomOffset"];
    pConfig->pictControl.encNumMBsPerSlice = configTable["encNumMBsPerSlice"];
    pConfig->pictControl.encNumSlicesPerFrame = configTable["encNumSlicesPerFrame"];
    pConfig->pictControl.encForceIntraRefresh = configTable["encForceIntraRefresh"];
    pConfig->pictControl.encForceIMBPeriod = configTable["encForceIMBPeriod"];
    pConfig->pictControl.encInsertVUIParam = configTable["encInsertVUIParam"];
    pConfig->pictControl.encInsertSEIMsg = configTable["encInsertSEIMsg"];

    /**************************************************************************/
    /* fill-in the rate control structures                                    */
    /**************************************************************************/
    pConfig->rateControl.size = sizeof(OVE_CONFIG_RATE_CONTROL);
    pConfig->rateControl.encRateControlMethod = configTable["encRateControlMethod"];
    pConfig->rateControl.encRateControlTargetBitRate = configTable["encRateControlTargetBitRate"];
    pConfig->rateControl.encRateControlPeakBitRate = configTable["encRateControlPeakBitRate"];
    pConfig->rateControl.encRateControlFrameRateNumerator = configTable["encRateControlFrameRateNumerator"];
    pConfig->rateControl.encGOPSize = configTable["encGOPSize"];
    pConfig->rateControl.encRCOptions = configTable["encRCOptions"];
    pConfig->rateControl.encQP_I = configTable["encQP_I"];
    pConfig->rateControl.encQP_P = configTable["encQP_P"];
    pConfig->rateControl.encQP_B = configTable["encQP_B"];
    pConfig->rateControl.encVBVBufferSize = configTable["encVBVBufferSize"];
    pConfig->rateControl.encRateControlFrameRateDenominator = configTable["encRateControlFrameRateDenominator"];

    /**************************************************************************/
    /* fill-in the motion estimation control structures                       */
    /**************************************************************************/
    pConfig->meControl.size = sizeof(OVE_CONFIG_MOTION_ESTIMATION);
    pConfig->meControl.imeDecimationSearch = configTable["IMEDecimationSearch"];
    pConfig->meControl.motionEstHalfPixel = configTable["motionEstHalfPixel"];
    pConfig->meControl.motionEstQuarterPixel = configTable["motionEstQuarterPixel"];
    pConfig->meControl.disableFavorPMVPoint = configTable["disableFavorPMVPoint"];
    pConfig->meControl.forceZeroPointCenter = configTable["forceZeroPointCenter"];
    pConfig->meControl.lsmVert = configTable["LSMVert"];
    pConfig->meControl.encSearchRangeX = configTable["encSearchRangeX"];
    pConfig->meControl.encSearchRangeY = configTable["encSearchRangeY"];
    pConfig->meControl.encSearch1RangeX = configTable["encSearch1RangeX"];
    pConfig->meControl.encSearch1RangeY = configTable["encSearch1RangeY"];
    pConfig->meControl.disable16x16Frame1 = configTable["disable16x16Frame1"];
    pConfig->meControl.disableSATD = configTable["disableSATD"];
    pConfig->meControl.enableAMD = configTable["enableAMD"];
    pConfig->meControl.encDisableSubMode = configTable["encDisableSubMode"];
    pConfig->meControl.encIMESkipX = configTable["encIMESkipX"];
    pConfig->meControl.encIMESkipY = configTable["encIMESkipY"];
    pConfig->meControl.encEnImeOverwDisSubm = configTable["encEnImeOverwDisSubm"];
    pConfig->meControl.encImeOverwDisSubmNo = configTable["encImeOverwDisSubmNo"];
    pConfig->meControl.encIME2SearchRangeX = configTable["encIME2SearchRangeX"];
    pConfig->meControl.encIME2SearchRangeY = configTable["encIME2SearchRangeY"];

    /**************************************************************************/
    /* fill-in the RDO control structures                                     */
    /**************************************************************************/
    pConfig->rdoControl.size = sizeof(OVE_CONFIG_RDO);
    pConfig->rdoControl.encDisableTbePredIFrame = configTable["encDisableTbePredIFrame"];
    pConfig->rdoControl.encDisableTbePredPFrame = configTable["encDisableTbePredPFrame"];
    pConfig->rdoControl.useFmeInterpolY = configTable["useFmeInterpolY"];
    pConfig->rdoControl.useFmeInterpolUV = configTable["useFmeInterpolUV"];
    pConfig->rdoControl.enc16x16CostAdj = configTable["enc16x16CostAdj"];
    pConfig->rdoControl.encSkipCostAdj = configTable["encSkipCostAdj"];
    pConfig->rdoControl.encForce16x16skip = (uint8_t)configTable["encForce16x16skip"];
}

//TODO Does VCE support dynamically changing encode settings?
bool setEncodeConfig(ove_session session, OvConfigCtrl *pConfig)
{
    uint32_t   numOfConfigBuffers = 4;
    OVE_CONFIG configBuffers[4];
    OVresult res = 0;

    /**************************************************************************/
    /* send configuration values for this session                             */
    /**************************************************************************/
	//!!! OVEncodeSendConfig replaces (some) values in pConfig with actually used ones.
    configBuffers[0].config.pPictureControl = &(pConfig->pictControl);
    configBuffers[0].configType = OVE_CONFIG_TYPE_PICTURE_CONTROL;
    configBuffers[1].config.pRateControl = &(pConfig->rateControl);
    configBuffers[1].configType = OVE_CONFIG_TYPE_RATE_CONTROL;
    configBuffers[2].config.pMotionEstimation = &(pConfig->meControl);
    configBuffers[2].configType = OVE_CONFIG_TYPE_MOTION_ESTIMATION;
    configBuffers[3].config.pRDO = &(pConfig->rdoControl);
    configBuffers[3].configType = OVE_CONFIG_TYPE_RDO;
    res = OVEncodeSendConfig(session, numOfConfigBuffers, configBuffers);
    if (!res)
    {
        VCELog(TEXT("OVEncodeSendConfig returned error"));
        return false;
    }

    /**************************************************************************/
    /* Just verifying that the values have been set in the                    */
    /* encoding engine.                                                       */
    /**************************************************************************/
    OVE_CONFIG_PICTURE_CONTROL      pictureControlConfig;
    OVE_CONFIG_RATE_CONTROL         rateControlConfig;
    OVE_CONFIG_MOTION_ESTIMATION    meControlConfig;
    OVE_CONFIG_RDO                  rdoControlConfig;

    /**************************************************************************/
    /* get the picture control configuration.                                 */
    /**************************************************************************/
    memset(&pictureControlConfig, 0, sizeof(OVE_CONFIG_PICTURE_CONTROL));
    pictureControlConfig.size = sizeof(OVE_CONFIG_PICTURE_CONTROL);
    res = OVEncodeGetPictureControlConfig(session, &pictureControlConfig);
    CHECKCONFIG(pictureControlConfig.cabacEnable, pConfig->pictControl.cabacEnable);
    CHECKCONFIG(pictureControlConfig.encForceIntraRefresh, pConfig->pictControl.encForceIntraRefresh);
    CHECKCONFIG(pictureControlConfig.encForceIMBPeriod, pConfig->pictControl.encForceIMBPeriod);
    CHECKCONFIG(pictureControlConfig.encIPicPeriod, pConfig->pictControl.encIPicPeriod);
    CHECKCONFIG(pictureControlConfig.encIDRPeriod, pConfig->pictControl.encIDRPeriod);
    CHECKCONFIG(pictureControlConfig.useConstrainedIntraPred, pConfig->pictControl.useConstrainedIntraPred);


    /**************************************************************************/
    /* get the rate control configuration                                     */
    /**************************************************************************/
    memset(&rateControlConfig, 0, sizeof(OVE_CONFIG_RATE_CONTROL));
    rateControlConfig.size = sizeof(OVE_CONFIG_RATE_CONTROL);
    res = OVEncodeGetRateControlConfig(session, &rateControlConfig);
    if (res)
    {
        VCELog(TEXT("Active bitrate: %d"), rateControlConfig.encRateControlTargetBitRate);
        VCELog(TEXT("Active peak bitrate: %d"), rateControlConfig.encRateControlPeakBitRate);
        VCELog(TEXT("Active control method: %d"), rateControlConfig.encRateControlMethod);
        VCELog(TEXT("Active VBV buffer: %d"), rateControlConfig.encVBVBufferSize);
        VCELog(TEXT("Active QP for I/P/B: %d/%d/%d"), rateControlConfig.encQP_I, 
            rateControlConfig.encQP_P, rateControlConfig.encQP_B);
    }
    else 
    {
        VCELog(TEXT("OVEncodeGetRateControlConfig failed."));
    }

    /**************************************************************************/
    /* get the MotionEstimation configuration                                 */
    /**************************************************************************/
    memset(&meControlConfig, 0, sizeof(OVE_CONFIG_MOTION_ESTIMATION));
    meControlConfig.size = sizeof(OVE_CONFIG_MOTION_ESTIMATION);
    res = OVEncodeGetMotionEstimationConfig(session, &meControlConfig);
    CHECKCONFIG(meControlConfig.encDisableSubMode, pConfig->meControl.encDisableSubMode);
    CHECKCONFIG(meControlConfig.enableAMD, pConfig->meControl.enableAMD);
    CHECKCONFIG(meControlConfig.encSearchRangeX, pConfig->meControl.encSearchRangeX);
    CHECKCONFIG(meControlConfig.encSearchRangeY, pConfig->meControl.encSearchRangeY);
    CHECKCONFIG(meControlConfig.disableFavorPMVPoint, pConfig->meControl.disableFavorPMVPoint);
    CHECKCONFIG(meControlConfig.motionEstHalfPixel, pConfig->meControl.motionEstHalfPixel);
    CHECKCONFIG(meControlConfig.motionEstQuarterPixel, pConfig->meControl.motionEstQuarterPixel);


    /**************************************************************************/
    /* get the RDO configuration                                              */
    /**************************************************************************/
    memset(&rdoControlConfig, 0, sizeof(OVE_CONFIG_RDO));
    rdoControlConfig.size = sizeof(OVE_CONFIG_RDO);
    res = OVEncodeGetRDOControlConfig(session, &rdoControlConfig);
    CHECKCONFIG(rdoControlConfig.enc16x16CostAdj, pConfig->rdoControl.enc16x16CostAdj);
    CHECKCONFIG(rdoControlConfig.encDisableTbePredIFrame, pConfig->rdoControl.encDisableTbePredIFrame);
    CHECKCONFIG(rdoControlConfig.encDisableTbePredPFrame, pConfig->rdoControl.encDisableTbePredPFrame);
    CHECKCONFIG(rdoControlConfig.encForce16x16skip, pConfig->rdoControl.encForce16x16skip);
    CHECKCONFIG(rdoControlConfig.encSkipCostAdj, pConfig->rdoControl.encSkipCostAdj);
    CHECKCONFIG(rdoControlConfig.useFmeInterpolUV, pConfig->rdoControl.useFmeInterpolUV);
    CHECKCONFIG(rdoControlConfig.useFmeInterpolY, pConfig->rdoControl.useFmeInterpolY);

    return(res);
}