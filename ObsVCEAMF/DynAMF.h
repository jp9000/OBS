#pragma once
#include <amf\components\VideoEncoderVCE.h>

extern decltype(&AMFCreateContext) pAMFCreateContext;
extern decltype(&AMFCreateComponent) pAMFCreateComponent;

typedef AMF_RESULT(AMF_CDECL_CALL *AMFVariantInitType)(amf::AMFVariantStruct* pVariant);
typedef AMF_RESULT (AMF_CDECL_CALL *AMFVariantClearType)(amf::AMFVariantStruct* pVariant);
typedef AMF_RESULT (AMF_CDECL_CALL *AMFVariantCompareType)(const amf::AMFVariantStruct* pFirst, const amf::AMFVariantStruct* pSecond, bool& equal);
typedef AMF_RESULT (AMF_CDECL_CALL *AMFVariantCopyType)(amf::AMFVariantStruct* pDest, const amf::AMFVariantStruct* pSrc);
typedef AMF_RESULT (AMF_CDECL_CALL *AMFVariantChangeTypeType)(amf::AMFVariantStruct* pDest, const amf::AMFVariantStruct* pSrc, amf::AMF_VARIANT_TYPE newType);

typedef AMF_RESULT (AMF_CDECL_CALL *AMFVariantAssignBoolType)(amf::AMFVariantStruct* pDest, bool value);
typedef AMF_RESULT (AMF_CDECL_CALL *AMFVariantAssignInt64Type)(amf::AMFVariantStruct* pDest, amf_int64 value);
typedef AMF_RESULT (AMF_CDECL_CALL *AMFVariantAssignDoubleType)(amf::AMFVariantStruct* pDest, amf_double value);
typedef AMF_RESULT (AMF_CDECL_CALL *AMFVariantAssignStringType)(amf::AMFVariantStruct* pDest, const char* value);
typedef AMF_RESULT (AMF_CDECL_CALL *AMFVariantAssignWStringType)(amf::AMFVariantStruct* pDest, const wchar_t* value);
typedef AMF_RESULT (AMF_CDECL_CALL *AMFVariantAssignInterfaceType)(amf::AMFVariantStruct* pDest, amf::AMFInterface* value);

typedef AMF_RESULT (AMF_CDECL_CALL *AMFVariantAssignRectType)(amf::AMFVariantStruct* pDest, const AMFRect& value);
typedef AMF_RESULT (AMF_CDECL_CALL *AMFVariantAssignSizeType)(amf::AMFVariantStruct* pDest, const AMFSize& value);
typedef AMF_RESULT (AMF_CDECL_CALL *AMFVariantAssignPointType)(amf::AMFVariantStruct* pDest, const AMFPoint& value);
typedef AMF_RESULT (AMF_CDECL_CALL *AMFVariantAssignRateType)(amf::AMFVariantStruct* pDest, const AMFRate& value);
typedef AMF_RESULT (AMF_CDECL_CALL *AMFVariantAssignRatioType)(amf::AMFVariantStruct* pDest, const AMFRatio& value);
typedef AMF_RESULT (AMF_CDECL_CALL *AMFVariantAssignColorType)(amf::AMFVariantStruct* pDest, const AMFColor& value);

//helpers
typedef char* (AMF_CDECL_CALL *AMFVariantDuplicateStringType)(const char* from);
typedef void (AMF_CDECL_CALL *AMFVariantFreeStringType)(char* from);

typedef wchar_t* (AMF_CDECL_CALL *AMFVariantDuplicateWStringType)(const wchar_t* from);
typedef void (AMF_CDECL_CALL *AMFVariantFreeWStringType)(wchar_t* from);

#define FUNCDECL(fun) extern "C"  fun ## Type fun
//#define FUNCDECL(fun) extern "C"  decltype(fun) fun

FUNCDECL(AMFVariantInit);
FUNCDECL(AMFVariantClear);
FUNCDECL(AMFVariantCompare);
FUNCDECL(AMFVariantCopy);
FUNCDECL(AMFVariantChangeType);

FUNCDECL(AMFVariantAssignBool);
FUNCDECL(AMFVariantAssignInt64);
FUNCDECL(AMFVariantAssignDouble);
FUNCDECL(AMFVariantAssignString);
FUNCDECL(AMFVariantAssignWString);
FUNCDECL(AMFVariantAssignInterface);

FUNCDECL(AMFVariantAssignRect);
FUNCDECL(AMFVariantAssignSize);
FUNCDECL(AMFVariantAssignPoint);
FUNCDECL(AMFVariantAssignRate);
FUNCDECL(AMFVariantAssignRatio);
FUNCDECL(AMFVariantAssignColor);

//helpers
FUNCDECL(AMFVariantDuplicateString);
FUNCDECL(AMFVariantFreeString);

FUNCDECL(AMFVariantDuplicateWString);
FUNCDECL(AMFVariantFreeWString);

#undef FUNCDECL

bool BindAMF();
void UnbindAMF();
