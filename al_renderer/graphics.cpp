#include "stdafx.h"
#include "win32app.h"
#include "graphics.h"
#include "text_buf.h"
#include "graphics\imgui_render.h"
#include "util.h"
#include "imgui\imgui.h"
static uint2 s_renderSize;
static float s_aspectRatio;

// Pipeline objects.
D3DPtr<ID3D12Device> g_d3dDevice;

static D3D12_VIEWPORT s_viewport;
static D3D12_RECT s_scissorRect;
D3DPtr<IDXGISwapChain3> g_swapChain;
D3DPtr<ID3D12Resource> g_renderTargets[FrameCount];
D3DPtr<ID3D12CommandAllocator> g_commandAllocator;
D3DPtr<ID3D12CommandQueue> g_commandQueue;
D3DPtr<ID3D12RootSignature> g_rootSignature;
D3DPtr<ID3D12DescriptorHeap> g_rtvHeap;
D3DPtr<ID3D12DescriptorHeap> g_cbvSrvUavHeap;
D3DPtr<ID3D12PipelineState> g_pipelineState;
D3DPtr<ID3D12GraphicsCommandList> g_d3dCommandList;
static u32 s_cbvSrvUavDescriptorNext;
static u32 s_cbvSrvUavDescriptorSize;
static u32 s_rtvDescriptorSize;

// Synchronization objects.
u32 g_frameIndex;
static HANDLE s_fenceEvent;
static D3DPtr<ID3D12Fence> s_fence;
static u64 s_frameNumber;
static constexpr u32 FRAME_RESOURCE_COUNT = 1;

struct
{
	TaggedFenceHeap_s heap;
	TaggedAllocator_s alloc[2];
	D3DPtr< ID3D12Resource > resource;
} s_uploadHeap;

const UploadBufferAllocHandle_t g_uploadBufferAlloc_perFrameVtx = 0;
const UploadBufferAllocHandle_t g_uploadBufferAlloc_perFrameConst = 1;

void GetHardwareAdapter( IDXGIFactory2* pFactory, IDXGIAdapter1** ppAdapter )
{
	D3DPtr<IDXGIAdapter1> adapter;
	*ppAdapter = nullptr;

	for ( UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != pFactory->EnumAdapters1( adapterIndex, &adapter ); ++adapterIndex )
	{
		DXGI_ADAPTER_DESC1 desc;
		adapter->GetDesc1( &desc );

		// Don't select the Basic Render Driver adapter.
		// If you want a software adapter, pass in "/warp" on the command line.
		if ( desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE )
			continue;

		// Check to see if the adapter supports Direct3D 12, but don't create the
		// actual device yet.
		if ( SUCCEEDED( D3D12CreateDevice( adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof( ID3D12Device ), nullptr ) ) )
			break;
	}

	*ppAdapter = adapter.Detach();
}

void WaitForPreviousFrame()
{
	// WAITING FOR THE FRAME TO COMPLETE BEFORE CONTINUING IS NOT BEST PRACTICE.
	// This is code implemented as such for simplicity. The D3D12HelloFrameBuffering
	// sample illustrates how to use fences for efficient resource usage and to
	// maximize GPU utilization.

	// Signal and increment the fence value.
	const UINT64 fence = s_frameNumber;
	D3D_Verify( g_commandQueue->Signal( s_fence.Get(), fence ) );
	s_frameNumber++;

	// Wait until the previous frame is finished.
	if ( s_fence->GetCompletedValue() < fence )
	{
		D3D_Verify( s_fence->SetEventOnCompletion( fence, s_fenceEvent ) );
		WaitForSingleObject( s_fenceEvent, INFINITE );
	}

	g_frameIndex = g_swapChain->GetCurrentBackBufferIndex();
}

void Graphics_Init( u32 width, u32 height )
{
	s_viewport = { 0.0f, 0.0f, static_cast< float >( width ), static_cast< float >( height ) };
	s_scissorRect = { 0, 0, static_cast< LONG >( width ), static_cast< LONG >( height ) };
	s_renderSize.xm = { width, height };
	s_aspectRatio = static_cast< float >( width ) / static_cast< float >( height );

	UINT dxgiFactoryFlags = 0;

#if defined(_DEBUG)
	// Enable the debug layer (requires the Graphics Tools "optional feature").
	// NOTE: Enabling the debug layer after device creation will invalidate the active device.
	{
		D3DPtr<ID3D12Debug> debugController;
		if ( SUCCEEDED( D3D12GetDebugInterface( IID_PPV_ARGS( &debugController ) ) ) )
		{
			debugController->EnableDebugLayer();

			// Enable additional debug layers.
			dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
		}
	}
#endif

	D3DPtr<IDXGIFactory4> factory;
	D3D_Verify( CreateDXGIFactory2( dxgiFactoryFlags, IID_PPV_ARGS( &factory ) ) );

	D3DPtr<IDXGIAdapter1> hardwareAdapter;
	GetHardwareAdapter( factory.Get(), &hardwareAdapter );

	D3D_Verify( D3D12CreateDevice(
		hardwareAdapter.Get(),
		D3D_FEATURE_LEVEL_11_0,
		IID_PPV_ARGS( &g_d3dDevice )
	) );

	// Describe and create the command queue.
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	D3D_Verify( g_d3dDevice->CreateCommandQueue( &queueDesc, IID_PPV_ARGS( &g_commandQueue ) ) );

	// Describe and create the swap chain.
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.BufferCount = FrameCount;
	swapChainDesc.Width = s_renderSize.x;
	swapChainDesc.Height = s_renderSize.y;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.SampleDesc.Count = 1;

	D3DPtr<IDXGISwapChain1> swapChain;
	D3D_Verify( factory->CreateSwapChainForHwnd(
		g_commandQueue.Get(),		// Swap chain needs the queue so that it can force a flush on it.
		g_hwnd,
		&swapChainDesc,
		nullptr,
		nullptr,
		&swapChain
	) );

	// This sample does not support fullscreen transitions.
	D3D_Verify( factory->MakeWindowAssociation( g_hwnd, DXGI_MWA_NO_ALT_ENTER ) );

	swapChain->QueryInterface( __uuidof( *g_swapChain.Get() ), reinterpret_cast<void**>( &g_swapChain ) );
	g_frameIndex = g_swapChain->GetCurrentBackBufferIndex();

	// Create descriptor heaps.
	{
		// Describe and create a render target view (RTV) descriptor heap.
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
		rtvHeapDesc.NumDescriptors = FrameCount;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		D3D_Verify( g_d3dDevice->CreateDescriptorHeap( &rtvHeapDesc, IID_PPV_ARGS( &g_rtvHeap ) ) );
		s_rtvDescriptorSize = g_d3dDevice->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_RTV );

		D3D12_DESCRIPTOR_HEAP_DESC cbvSrvUavHeapDesc;
		cbvSrvUavHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		cbvSrvUavHeapDesc.NodeMask = 0;
		cbvSrvUavHeapDesc.NumDescriptors = 10;
		cbvSrvUavHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		D3D_Verify( g_d3dDevice->CreateDescriptorHeap( &cbvSrvUavHeapDesc, IID_PPV_ARGS( &g_cbvSrvUavHeap ) ) );
		s_cbvSrvUavDescriptorSize = g_d3dDevice->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );
	}

	// Create frame resources.
	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle( g_rtvHeap->GetCPUDescriptorHandleForHeapStart() );

		// Create a RTV for each frame.
		for ( UINT n = 0; n < FrameCount; n++ )
		{
			D3D_Verify( g_swapChain->GetBuffer( n, IID_PPV_ARGS( &g_renderTargets[n] ) ) );
			g_d3dDevice->CreateRenderTargetView( g_renderTargets[n].Get(), nullptr, rtvHandle );
			rtvHandle.Offset( 1, s_rtvDescriptorSize );
		}
	}

	D3D_Verify( g_d3dDevice->CreateCommandAllocator( D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS( &g_commandAllocator ) ) );

	// Create an empty root signature.
	{
		const CD3DX12_STATIC_SAMPLER_DESC staticSamplers[] =
		{
			// Applications usually only need a handful of samplers.  So just define them all up front
			// and keep them available as part of the root signature.  
			CD3DX12_STATIC_SAMPLER_DESC(
				0, // shaderRegister
				D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
				D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
				D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
				D3D12_TEXTURE_ADDRESS_MODE_WRAP ), // addressW

			CD3DX12_STATIC_SAMPLER_DESC(
				1, // shaderRegister
				D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
				D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
				D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
				D3D12_TEXTURE_ADDRESS_MODE_CLAMP ), // addressW

			CD3DX12_STATIC_SAMPLER_DESC(
				2, // shaderRegister
				D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
				D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
				D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
				D3D12_TEXTURE_ADDRESS_MODE_WRAP ), // addressW

			CD3DX12_STATIC_SAMPLER_DESC(
				3, // shaderRegister
				D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
				D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
				D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
				D3D12_TEXTURE_ADDRESS_MODE_CLAMP ), // addressW

			CD3DX12_STATIC_SAMPLER_DESC(
				4, // shaderRegister
				D3D12_FILTER_ANISOTROPIC, // filter
				D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
				D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
				D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
				0.0f,                             // mipLODBias
				8 ),                               // maxAnisotropy

			CD3DX12_STATIC_SAMPLER_DESC(
				5, // shaderRegister
				D3D12_FILTER_ANISOTROPIC, // filter
				D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
				D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
				D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
				0.0f,                              // mipLODBias
				8 ),                                // maxAnisotropy

			CD3DX12_STATIC_SAMPLER_DESC(
				6, // shaderRegister
				D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT, // filter
				D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressU
				D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressV
				D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressW
				0.0f,                              // mipLODBias
				16,
				D3D12_COMPARISON_FUNC_LESS_EQUAL,
				D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK ), // maxAnisotropy
		};

		CD3DX12_DESCRIPTOR_RANGE descRanges[10];
		descRanges[0].Init( D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 10, 0 );
		descRanges[1].Init( D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 8, 1 );
		descRanges[2].Init( D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 10, 0 );
		descRanges[3].Init( D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 8, 1 );
		descRanges[4].Init( D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 10, 0 );
		descRanges[5].Init( D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 8, 1 );
		descRanges[6].Init( D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 10, 0 );
		descRanges[7].Init( D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 8, 1 );
		descRanges[8].Init( D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 5, 10 );
		descRanges[9].Init( D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 3, 9 );


		CD3DX12_ROOT_PARAMETER params[11];
		//params[ToIdx( Graphics_RootParams::ALL_ROOT_CBV )].InitAsConstantBufferView( 0, 0, D3D12_SHADER_VISIBILITY_ALL );
		params[ToIdx( Graphics_RootParams::ALL_ROOT_CBV )].InitAsConstants( 16, 0, D3D12_SHADER_VISIBILITY_ALL );
		params[ToIdx( Graphics_RootParams::PIXEL_SRV )].InitAsDescriptorTable( 1, &descRanges[0], D3D12_SHADER_VISIBILITY_PIXEL );
		params[ToIdx( Graphics_RootParams::PIXEL_CBV )].InitAsDescriptorTable( 1, &descRanges[1], D3D12_SHADER_VISIBILITY_PIXEL );
		params[ToIdx( Graphics_RootParams::VERTEX_SRV )].InitAsDescriptorTable( 1, &descRanges[2], D3D12_SHADER_VISIBILITY_VERTEX );
		params[ToIdx( Graphics_RootParams::VERTEX_CBV )].InitAsDescriptorTable( 1, &descRanges[3], D3D12_SHADER_VISIBILITY_VERTEX );
		params[ToIdx( Graphics_RootParams::HULL_SRV )].InitAsDescriptorTable( 1, &descRanges[4], D3D12_SHADER_VISIBILITY_HULL );
		params[ToIdx( Graphics_RootParams::HULL_CBV )].InitAsDescriptorTable( 1, &descRanges[5], D3D12_SHADER_VISIBILITY_HULL );
		params[ToIdx( Graphics_RootParams::DOMAIN_SRV )].InitAsDescriptorTable( 1, &descRanges[6], D3D12_SHADER_VISIBILITY_DOMAIN );
		params[ToIdx( Graphics_RootParams::DOMAIN_CBV )].InitAsDescriptorTable( 1, &descRanges[7], D3D12_SHADER_VISIBILITY_DOMAIN );
		params[ToIdx( Graphics_RootParams::ALL_SRV )].InitAsDescriptorTable( 1, &descRanges[8], D3D12_SHADER_VISIBILITY_ALL );
		params[ToIdx( Graphics_RootParams::ALL_CBV )].InitAsDescriptorTable( 1, &descRanges[9], D3D12_SHADER_VISIBILITY_ALL );

		CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
		rootSignatureDesc.Init( _countof( params ), params, _countof( staticSamplers ), staticSamplers, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT );

		D3DPtr<ID3DBlob> signature;
		D3DPtr<ID3DBlob> error;
		HRESULT res = D3D12SerializeRootSignature( &rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error );

		if ( error.IsValid() )
			::OutputDebugStringA( reinterpret_cast< char* >(error->GetBufferPointer()) );

		D3D_Verify( res );
		D3D_Verify( g_d3dDevice->CreateRootSignature( 0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS( &g_rootSignature ) ) );
	}

	// Create the pipeline state, which includes compiling and loading shaders.
	{
		D3DPtr<ID3DBlob> vertexShader;
		D3DPtr<ID3DBlob> pixelShader;

#if defined(_DEBUG)
		// Enable better shader debugging with the graphics debugging tools.
		UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		UINT compileFlags = 0;
#endif

		D3D_Verify( D3DCompileFromFile( L"shaders/basic.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, nullptr ) );
		D3D_Verify( D3DCompileFromFile( L"shaders/basic.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, nullptr ) );

		// Define the vertex input layout.
		D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		// Describe and create the graphics pipeline state object (PSO).
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.InputLayout = { inputElementDescs, _countof( inputElementDescs ) };
		psoDesc.pRootSignature = g_rootSignature.Get();
		psoDesc.VS = CD3DX12_SHADER_BYTECODE( vertexShader.Get() );
		psoDesc.PS = CD3DX12_SHADER_BYTECODE( pixelShader.Get() );
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC( D3D12_DEFAULT );
		psoDesc.BlendState = CD3DX12_BLEND_DESC( D3D12_DEFAULT );
		psoDesc.DepthStencilState.DepthEnable = FALSE;
		psoDesc.DepthStencilState.StencilEnable = FALSE;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.SampleDesc.Count = 1;
		D3D_Verify( g_d3dDevice->CreateGraphicsPipelineState( &psoDesc, IID_PPV_ARGS( &g_pipelineState ) ) );
	}

	// Create the command list.
	D3D_Verify( g_d3dDevice->CreateCommandList( 0, D3D12_COMMAND_LIST_TYPE_DIRECT, g_commandAllocator.Get(), g_pipelineState.Get(), IID_PPV_ARGS( &g_d3dCommandList ) ) );

	// Create synchronization objects and wait until assets have been uploaded to the GPU.
	D3D_Verify( g_d3dDevice->CreateFence( 0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS( &s_fence ) ) );
	s_frameNumber = 1;

	// Create an event handle to use for frame synchronization.
	s_fenceEvent = CreateEvent( nullptr, FALSE, FALSE, nullptr );
	if ( s_fenceEvent == nullptr )
		D3D_Verify( HRESULT_FROM_WIN32( GetLastError() ) );

	// Create upload resources
	{
		constexpr u32 UPLOAD_BLOCK_COUNT = 64;
		constexpr u32 UPLOAD_BLOCK_SIZE = 2 * MB;
		constexpr u32 UPLOAD_BUFFER_SIZE = UPLOAD_BLOCK_SIZE * UPLOAD_BLOCK_COUNT;
		D3D12_RESOURCE_DESC desc;
		memset( &desc, 0, sizeof( D3D12_RESOURCE_DESC ) );
		desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		desc.Width = UPLOAD_BUFFER_SIZE;
		desc.Height = 1;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 1;
		desc.Format = DXGI_FORMAT_UNKNOWN;
		desc.SampleDesc.Count = 1;
		desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		desc.Flags = D3D12_RESOURCE_FLAG_NONE;
		D3D_Verify( g_d3dDevice->CreateCommittedResource( &CD3DX12_HEAP_PROPERTIES{ D3D12_HEAP_TYPE_UPLOAD }, D3D12_HEAP_FLAG_NONE,
			&desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS( &s_uploadHeap.resource ) ) );

		CD3DX12_RANGE readRange{ 0, 0 };
		void* mappedData;
		s_uploadHeap.resource->Map( 0, &readRange, &mappedData );
		TaggedFenceHeap_Init( UPLOAD_BLOCK_SIZE, UPLOAD_BLOCK_COUNT, mappedData, &s_uploadHeap.heap );
		TaggedAlloc_Init( &s_uploadHeap.alloc[g_uploadBufferAlloc_perFrameConst] );
		TaggedAlloc_Init( &s_uploadHeap.alloc[g_uploadBufferAlloc_perFrameVtx] );
	}

	g_ImguiRender_api.init();

	// Command lists are created in the recording state, but there is nothing
	// to record yet. The main loop expects it to be closed, so close it now.
	D3D_Verify( g_d3dCommandList->Close() );

	ID3D12CommandList* cmdLists[] = { g_d3dCommandList.Get() };
	g_commandQueue->ExecuteCommandLists( _countof( cmdLists ), cmdLists );

	// Wait for the command list to execute; we are reusing the same command 
	// list in our main loop but for now, we just want to wait for setup to 
	// complete before continuing.
	WaitForPreviousFrame();
}

// Render the scene.
void Graphics_Render()
{
	// Command list allocators can only be reset when the associated 
	// command lists have finished execution on the GPU; apps should use 
	// fences to determine GPU execution progress.
	D3D_Verify( g_commandAllocator->Reset() );

	// However, when ExecuteCommandList() is called on a particular command 
	// list, that command list can then be reset at any time and must be before 
	// re-recording.
	D3D_Verify( g_d3dCommandList->Reset( g_commandAllocator.Get(), g_pipelineState.Get() ) );

	// Set necessary state.
	g_d3dCommandList->SetGraphicsRootSignature( g_rootSignature.Get() );
	g_d3dCommandList->RSSetViewports( 1, &s_viewport );
	g_d3dCommandList->RSSetScissorRects( 1, &s_scissorRect );
	g_d3dCommandList->SetDescriptorHeaps( 1, &g_cbvSrvUavHeap );

	// Indicate that the back buffer will be used as a render target.
	g_d3dCommandList->ResourceBarrier( 1, &CD3DX12_RESOURCE_BARRIER::Transition( g_renderTargets[g_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET ) );

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle( g_rtvHeap->GetCPUDescriptorHandleForHeapStart(), g_frameIndex, s_rtvDescriptorSize );
	g_d3dCommandList->OMSetRenderTargets( 1, &rtvHandle, FALSE, nullptr );

	// Record commands.
	const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
	g_d3dCommandList->ClearRenderTargetView( rtvHandle, clearColor, 0, nullptr );

	g_ImguiRender_api.render();

	// Indicate that the back buffer will now be used to present.
	g_d3dCommandList->ResourceBarrier( 1, &CD3DX12_RESOURCE_BARRIER::Transition( g_renderTargets[g_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT ) );

	D3D_Verify( g_d3dCommandList->Close() );

	// Execute the command list.
	ID3D12CommandList* ppCommandLists[] = { g_d3dCommandList.Get() };
	g_commandQueue->ExecuteCommandLists( _countof( ppCommandLists ), ppCommandLists );

	{
		const u64 fenceValue = s_uploadHeap.heap.nextFence++;
		g_commandQueue->Signal( s_uploadHeap.heap.fence.Get(), fenceValue );
		TaggedFenceHeap_Free( &s_uploadHeap.heap, &s_uploadHeap.alloc[g_uploadBufferAlloc_perFrameConst], fenceValue );
		TaggedFenceHeap_Free( &s_uploadHeap.heap, &s_uploadHeap.alloc[g_uploadBufferAlloc_perFrameVtx], fenceValue );
	}

	// Present the frame.
	D3D_Verify( g_swapChain->Present( 1, 0 ) );


	WaitForPreviousFrame();
}

float3 operator-(float3 l, float3 r)
{
	return float3{ l.x - r.x, l.y - r.y, l.z - r.z };
}

float3 operator+(float3 l, float3 r)
{
	return float3{ l.x + r.x, l.y + r.y, l.z + r.z };
}

float dot(float3 l, float3 r)
{
	return l.x * r.x + l.y * r.y + l.z * r.z;
}

float3 operator*(float l, float3 r)
{
	return float3{ l * r.x, l * r.y, l * r.z };
}

float dist(float3 x, float3 s, float3 e)
{
	float3 L_v = e - s;
	float3 v = s - x;
	float k = -dot(L_v, v) / dot(L_v, L_v);
	k = min(1.0f, max(0.0f, k));
	return sqrt(dot(v + (k * L_v), v + (k * L_v)));
}

void Graphics_Update()
{

	ImGui::Text( "Distance: %f", dist(float3{ 0,-3,4 }, float3{ 0,-3,0 }, float3{ 5,-3,0 }));


	g_ImguiRender_api.update();
}

void Graphics_Destroy()
{
	// Ensure that the GPU is no longer referencing resources that are about to be
	// cleaned up by the destructor.
	WaitForPreviousFrame();
	g_ImguiRender_api.shutdown();
	CloseHandle( s_fenceEvent );
}

void Graphics_GetNextCbvSrtUavDescriptorHandle( D3D12_CPU_DESCRIPTOR_HANDLE* const cpuDesc, D3D12_GPU_DESCRIPTOR_HANDLE* const gpuDesc )
{
	cpuDesc->ptr = g_cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart().ptr + s_cbvSrvUavDescriptorNext * s_cbvSrvUavDescriptorSize;
	gpuDesc->ptr = g_cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart().ptr + s_cbvSrvUavDescriptorNext * s_cbvSrvUavDescriptorSize;
	++s_cbvSrvUavDescriptorNext;
}

void Graphics_MapUploadData( const UploadBufferAllocHandle_t handle, UploadBufferRange_s* const range )
{
	Assert( handle < _countof( s_uploadHeap.alloc ) );
	range->data = TaggedAlloc_Alloc( &s_uploadHeap.heap, &s_uploadHeap.alloc[handle], range->size, 0 );
	const u64 bufferOffset = static_cast< u8* >( range->data ) - static_cast< u8* >( s_uploadHeap.heap.data );
	range->gpuAddress = s_uploadHeap.resource->GetGPUVirtualAddress() + bufferOffset;
}