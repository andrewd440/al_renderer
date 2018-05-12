#pragma once

#include "tagged_heap.h"

#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxgi.lib")

struct RenderPlugin_s
{
	void( *init )();
	void( *update )();
	void( *render )();
	void( *shutdown )();
};

enum class Graphics_RootParams : u32
{
	ALL_ROOT_CBV,
	PIXEL_SRV,
	PIXEL_CBV,
	VERTEX_SRV,
	VERTEX_CBV,
	HULL_SRV,
	HULL_CBV,
	DOMAIN_SRV,
	DOMAIN_CBV,
	ALL_SRV,
	ALL_CBV,
};

void Graphics_Init( u32 width, u32 height );
void Graphics_Update();
void Graphics_Render();
void Graphics_Destroy();

void Graphics_GetNextCbvSrtUavDescriptorHandle( D3D12_CPU_DESCRIPTOR_HANDLE* cpuDesc, D3D12_GPU_DESCRIPTOR_HANDLE* gpuDesc );

struct MappedRange_s
{
	u64 size;

	D3D12_GPU_VIRTUAL_ADDRESS gpuAddress;
	void* data;
};

struct UploadBufferRange_s : public MappedRange_s {};
using UploadBufferAllocHandle_t = u32;
extern const UploadBufferAllocHandle_t g_uploadBufferAlloc_perFrameVtx;
extern const UploadBufferAllocHandle_t g_uploadBufferAlloc_perFrameConst;
void Graphics_MapUploadData( UploadBufferAllocHandle_t handle, UploadBufferRange_s* range );

static constexpr u32 FrameCount = 2;
extern u32 g_frameIndex;
extern D3DPtr<ID3D12Device> g_d3dDevice;
extern D3DPtr<ID3D12GraphicsCommandList> g_d3dCommandList;
extern D3DPtr<ID3D12CommandQueue> g_commandQueue;
extern D3DPtr<ID3D12RootSignature> g_rootSignature;