
#include "stdafx.h"
#include "basictypes.h"
#include "graphics.h"
#include "imgui\imgui.h"
#include "util.h"

static struct
{
	D3DPtr< ID3D12Resource > res;
	D3DPtr< ID3D12Resource > upload;
	D3D12_CPU_DESCRIPTOR_HANDLE cpuDesc;
	D3D12_GPU_DESCRIPTOR_HANDLE gpuDesc;
} s_textTex;

static D3DPtr< ID3D12PipelineState > s_pso;

struct ImGui_ConstBuf
{
	float4x4 mvp;
};

static void ImguiRender_Init();
static void ImguiRender_Update();
static void ImguiRender_Render();
static void ImguiRender_Shutdown();

RenderPlugin_s g_ImguiRender_api
{
	&ImguiRender_Init,
	&ImguiRender_Update,
	&ImguiRender_Render,
	&ImguiRender_Shutdown
};

static void ImguiRender_Init()
{
	D3DPtr<ID3DBlob> vertexShader = CompileShader( L"shaders/ui.hlsl", nullptr, "VSMain", "vs_5_0" );
	D3DPtr<ID3DBlob> pixelShader = CompileShader( L"shaders/ui.hlsl", nullptr, "PSMain", "ps_5_0" );

	const D3D12_INPUT_ELEMENT_DESC inputLayout[] = 
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,   0, offsetof( ImDrawVert, pos), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,   0, offsetof( ImDrawVert, uv),  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof( ImDrawVert, col), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	// Describe and create the graphics pipeline state object (PSO).
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.VS = CD3DX12_SHADER_BYTECODE( vertexShader.Get() );
	psoDesc.PS = CD3DX12_SHADER_BYTECODE( pixelShader.Get() );
	psoDesc.NodeMask = 1;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.InputLayout = { inputLayout, _countof( inputLayout ) };
	psoDesc.pRootSignature = g_rootSignature.Get();
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

	// Create the blending setup
	{
		D3D12_BLEND_DESC& desc = psoDesc.BlendState;
		desc.AlphaToCoverageEnable = false;
		desc.RenderTarget[0].BlendEnable = true;
		desc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
		desc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		desc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		desc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
		desc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
		desc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
		desc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	}

	// Create the rasterizer state
	{
		D3D12_RASTERIZER_DESC& desc = psoDesc.RasterizerState;
		desc.FillMode = D3D12_FILL_MODE_SOLID;
		desc.CullMode = D3D12_CULL_MODE_NONE;
		desc.FrontCounterClockwise = FALSE;
		desc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
		desc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
		desc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
		desc.DepthClipEnable = true;
		desc.MultisampleEnable = FALSE;
		desc.AntialiasedLineEnable = FALSE;
		desc.ForcedSampleCount = 0;
		desc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
	}

	// Create depth-stencil State
	{
		D3D12_DEPTH_STENCIL_DESC& desc = psoDesc.DepthStencilState;
		desc.DepthEnable = false;
		desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		desc.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		desc.StencilEnable = false;
		desc.FrontFace.StencilFailOp = desc.FrontFace.StencilDepthFailOp = desc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
		desc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		desc.BackFace = desc.FrontFace;
	}
	D3D_Verify( g_d3dDevice->CreateGraphicsPipelineState( &psoDesc, IID_PPV_ARGS( &s_pso ) ) );

	ImGui::CreateContext();
	ImGui::StyleColorsDark();

	ImGuiIO* const io = &ImGui::GetIO();
	io->DisplaySize = { 1280, 720 };

	{
		u8* pixels;
		i32 width, height, bytesPerPixel;
		io->Fonts->GetTexDataAsRGBA32( &pixels, &width, &height, &bytesPerPixel );

		D3D12_RESOURCE_DESC texDesc;
		texDesc.Alignment = 0;
		texDesc.DepthOrArraySize = 1;
		texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		texDesc.Height = height;
		texDesc.Width = width;
		texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		texDesc.MipLevels = 1;
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
		D3D_Verify( g_d3dDevice->CreateCommittedResource( &CD3DX12_HEAP_PROPERTIES{ D3D12_HEAP_TYPE_DEFAULT }, D3D12_HEAP_FLAG_NONE, &texDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS( &s_textTex.res ) ) );

		const uXX uploadPitch = Align< D3D12_TEXTURE_DATA_PITCH_ALIGNMENT >( size_cast< uXX >( width ) * 4ul * sizeof( u8 ) );
		D3D12_RESOURCE_DESC uploadDesc = texDesc;
		uploadDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		uploadDesc.Format = DXGI_FORMAT_UNKNOWN;
		uploadDesc.Height = 1;
		uploadDesc.Width = uploadPitch * height;
		uploadDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		
		D3D_Verify( g_d3dDevice->CreateCommittedResource( &CD3DX12_HEAP_PROPERTIES{ D3D12_HEAP_TYPE_UPLOAD }, D3D12_HEAP_FLAG_NONE, &uploadDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS( &s_textTex.upload ) ) );

		u8* mapped;
		D3D12_RANGE mapRange{ 0, uploadDesc.Width };
		s_textTex.upload->Map( 0, &mapRange, reinterpret_cast< void** >( &mapped ) );
		for ( u32 y = 0; y < height; ++y )
			memcpy( mapped + y * uploadPitch, pixels + y * width * 4, width * 4 );
		s_textTex.upload->Unmap( 0, &mapRange );

		D3D12_TEXTURE_COPY_LOCATION srcLocation = {};
		srcLocation.pResource = s_textTex.upload.Get();
		srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		srcLocation.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		srcLocation.PlacedFootprint.Footprint.Width = width;
		srcLocation.PlacedFootprint.Footprint.Height = height;
		srcLocation.PlacedFootprint.Footprint.Depth = 1;
		srcLocation.PlacedFootprint.Footprint.RowPitch = uploadPitch;

		D3D12_TEXTURE_COPY_LOCATION dstLocation = {};
		dstLocation.pResource = s_textTex.res.Get();
		dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		dstLocation.SubresourceIndex = 0;

		g_d3dCommandList->CopyTextureRegion( &dstLocation, 0, 0, 0, &srcLocation, nullptr );
		g_d3dCommandList->ResourceBarrier( 1, &CD3DX12_RESOURCE_BARRIER::Transition( s_textTex.res.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE ) );

		Graphics_GetNextCbvSrtUavDescriptorHandle( &s_textTex.cpuDesc, &s_textTex.gpuDesc );

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
		srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = texDesc.MipLevels;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.PlaneSlice = 0;
		srvDesc.Texture2D.ResourceMinLODClamp = 0;
		g_d3dDevice->CreateShaderResourceView( s_textTex.res.Get(), &srvDesc, s_textTex.cpuDesc );
	}

	io->Fonts->TexID = reinterpret_cast< void* >( s_textTex.gpuDesc.ptr );
	ImGui::NewFrame();
}

static void ImguiRender_Update()
{
	ImGui::ShowDemoWindow();
}

static void ImguiRender_Render()
{
	ImGui::Render();
	const ImDrawData* const drawData = ImGui::GetDrawData();

	ID3D12GraphicsCommandList* const ctx = g_d3dCommandList.Get();

	// Copy and convert all vertices into a single contiguous buffer
	UploadBufferRange_s vtxRange; vtxRange.size = drawData->TotalVtxCount * sizeof(ImDrawVert);
	UploadBufferRange_s idxRange; idxRange.size = drawData->TotalIdxCount * sizeof(ImDrawIdx);
	Graphics_MapUploadData( g_uploadBufferAlloc_perFrameVtx, &vtxRange );
	Graphics_MapUploadData( g_uploadBufferAlloc_perFrameVtx, &idxRange );

	ImDrawVert* vtx_dst = (ImDrawVert*)vtxRange.data;
	ImDrawIdx* idx_dst = (ImDrawIdx*)idxRange.data;
	for ( int n = 0; n < drawData->CmdListsCount; n++ )
	{
		const ImDrawList* cmd_list = drawData->CmdLists[n];
		memcpy( vtx_dst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof( ImDrawVert ) );
		memcpy( idx_dst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof( ImDrawIdx ) );
		vtx_dst += cmd_list->VtxBuffer.Size;
		idx_dst += cmd_list->IdxBuffer.Size;
	}

	// Setup orthographic projection matrix into our constant buffer
	ImGui_ConstBuf vertex_constant_buffer;
	{
		float L = 0.0f;
		float R = ImGui::GetIO().DisplaySize.x;
		float B = ImGui::GetIO().DisplaySize.y;
		float T = 0.0f;
		float mvp[4][4] =
		{
			{ 2.0f / (R - L),   0.0f,           0.0f,       0.0f },
			{ 0.0f,         2.0f / (T - B),     0.0f,       0.0f },
			{ 0.0f,         0.0f,           0.5f,       0.0f },
			{ (R + L) / (L - R),  (T + B) / (B - T),    0.5f,       1.0f },
		};
		memcpy( &vertex_constant_buffer.mvp, mvp, sizeof( mvp ) );
	}

	// Setup viewport
	D3D12_VIEWPORT vp;
	ZeroMemoryT( &vp );
	vp.Width = ImGui::GetIO().DisplaySize.x;
	vp.Height = ImGui::GetIO().DisplaySize.y;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = vp.TopLeftY = 0.0f;
	ctx->RSSetViewports( 1, &vp );

	// Bind shader and vertex buffers
	unsigned int stride = sizeof( ImDrawVert );
	unsigned int offset = 0;
	D3D12_VERTEX_BUFFER_VIEW vbv;
	ZeroMemoryT( &vbv );
	vbv.BufferLocation = vtxRange.gpuAddress;
	vbv.SizeInBytes = drawData->TotalVtxCount * sizeof( ImDrawVert );
	vbv.StrideInBytes = stride;
	ctx->IASetVertexBuffers( 0, 1, &vbv );
	D3D12_INDEX_BUFFER_VIEW ibv;
	memset( &ibv, 0, sizeof( D3D12_INDEX_BUFFER_VIEW ) );
	ibv.BufferLocation = idxRange.gpuAddress;
	ibv.SizeInBytes = drawData->TotalIdxCount * sizeof( ImDrawIdx );
	ibv.Format = sizeof( ImDrawIdx ) == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
	ctx->IASetIndexBuffer( &ibv );
	ctx->IASetPrimitiveTopology( D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
	ctx->SetPipelineState( s_pso.Get() );
	ctx->SetGraphicsRootSignature( g_rootSignature.Get() );
	ctx->SetGraphicsRoot32BitConstants( 0, 16, &vertex_constant_buffer, 0 );

	// Setup render state
	const float blend_factor[4] = { 0.f, 0.f, 0.f, 0.f };
	ctx->OMSetBlendFactor( blend_factor );

	// Render command lists
	int vtx_offset = 0;
	int idx_offset = 0;
	for ( int n = 0; n < drawData->CmdListsCount; n++ )
	{
		const ImDrawList* cmd_list = drawData->CmdLists[n];
		for ( int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++ )
		{
			const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
			if ( pcmd->UserCallback )
			{
				pcmd->UserCallback( cmd_list, pcmd );
			}
			else
			{
				const D3D12_RECT r = { (LONG)pcmd->ClipRect.x, (LONG)pcmd->ClipRect.y, (LONG)pcmd->ClipRect.z, (LONG)pcmd->ClipRect.w };
				ctx->SetGraphicsRootDescriptorTable( 1, *(D3D12_GPU_DESCRIPTOR_HANDLE*)&pcmd->TextureId );
				ctx->RSSetScissorRects( 1, &r );
				ctx->DrawIndexedInstanced( pcmd->ElemCount, 1, idx_offset, vtx_offset, 0 );
			}
			idx_offset += pcmd->ElemCount;
		}
		vtx_offset += cmd_list->VtxBuffer.Size;
	}

	ImGui::NewFrame();
}

static void ImguiRender_Shutdown()
{
	ImGui::DestroyContext();
}