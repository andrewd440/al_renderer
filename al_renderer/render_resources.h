#pragma once

#include "basictypes.h"

struct RenderBuffer_s
{

};

struct RenderTexture_s
{

};

struct RenderDescriptor_s
{

};

using VertexShader_t = u16;
using PixelShader_t = u16;
using GeometryShader_t = u16;
using TessShader_t = u16;

struct RenderShader_s
{
	VertexShader_t vs;
	PixelShader_t ps;
	GeometryShader_t gs;
	TessShader_t ts;


};