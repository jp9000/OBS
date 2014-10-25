#include <amf\core\Surface.h>

class Observer : public amf::AMFSurfaceObserver
{
public:
	/**
	********************************************************************************
	* @brief ReleaseResources is called before internal release resources.
	* The callback is called once, automatically removed after call.
	* those are done in specific Surface ReleasePlanes
	********************************************************************************
	*/
	virtual void AMF_STD_CALL OnSurfaceDataRelease(amf::AMFSurface* pSurface)
	{
		OSDebugOut(TEXT("Release surface %p\n"), pSurface);
		InterlockedDecrement(&refCount);
	}

	Observer() : refCount(0) {}
	virtual ~Observer() {}

	Observer *IncRef()
	{
		InterlockedIncrement(&refCount);
		return this;
	}

	bool Unused()
	{
		return refCount == 0;
	}
private:
	LONG refCount;
};

typedef struct InputBuffer
{
	uint8_t *pBuffer;
	amf::AMFBufferPtr pAMFBuffer;
	size_t size;
	//bool locked;
	LONG locked;
	uint32_t timestamp;

	//TODO separate bools for yuv planes maybe
	bool isMapped;
	cl_mem surface;

	uint8_t *yuv_host_ptr[2];
	cl_mem yuv_surfaces[2];
	size_t yuv_row_pitches[2];
	size_t uv_width;
	size_t uv_height;
	amf::AMFSurfacePtr amf_surface;
	//Observer observer;
	amf::AMF_MEMORY_TYPE mem_type;

	IDirect3DSurface9 *pSurface9;
	ID3D11Texture2D *pTex;
	LONG inUse;

} InputBuffer;

typedef struct OutputBuffer
{
	void *pBuffer;
	size_t size;
	uint64_t timestamp;
} OutputBuffer;

typedef struct OutputList
{
	List<BYTE> pBuffer;
	uint64_t timestamp;
} OutputList;
