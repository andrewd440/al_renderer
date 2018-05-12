#include "stdafx.h"

#include "tagged_heap.h"
#include "graphics.h"

void TaggedHeap_Init( const u32 blockSize, const u32 blockCount, void* const data, TaggedHeap_s* const out_heap )
{
	out_heap->blockSize = blockSize;
	out_heap->blockCount = blockCount;
	out_heap->data = data;
	out_heap->blockBitfield.xm = uint4_Zero.xm;
	out_heap->preAlloc = nullptr;
}

bool TaggedHeap_Alloc( TaggedHeap_s* const heap, TaggedAllocator_s* const alloc, const u32 index )
{
	std::lock_guard<std::mutex> lock( heap->mutex );

	if ( heap->preAlloc )
		heap->preAlloc( heap );

	uint4* const __restrict const bitfield = &heap->blockBitfield;

	if ( ::XMVector4EqualInt( bitfield->xm, uint4_Max.xm ) )
		return false;

	u32 bit_32 = *reinterpret_cast< u64* >( bitfield->v ) == ~0 ? 2 : 0;
	bit_32 += bitfield->v[bit_32] == ~0 ? 1 : 0;

	u32 bit_8 = *reinterpret_cast< u16* >( &bitfield->v[bit_32] ) == ~0 ? 2 : 0;
	bit_8 += *(reinterpret_cast< u8* >( &bitfield->v[bit_32] ) + bit_8) == ~0 ? 1 : 0;

	u8* __restrict const freeBitByte = (reinterpret_cast< u8* >( &bitfield->v[bit_32] ) + bit_8);
	Assert( *freeBitByte != ~0 );
	u32 bit_1 = 0;
	for ( ; bit_1 < 8; ++bit_1 )
	{
		if ( ((*freeBitByte >> bit_1) & 1) == 0 )
		{
			*freeBitByte |= (1 << bit_1);
			break;
		}
	}

	const u32 freeBlockIndex = bit_32 * 32 + bit_8 * 8 + bit_1;
	alloc->threadData[index] = static_cast< u8* >( heap->data ) + heap->blockSize * freeBlockIndex;
	alloc->threadDataHead[index] = 0;

	*(reinterpret_cast< u8* >( &alloc->tag.v[bit_32] ) + bit_8) |= (1 << bit_1);
	*(reinterpret_cast< u8* >( &heap->blockBitfield.v[bit_32] ) + bit_8) |= (1 << bit_1);

	return true;
}

void TaggedHeap_Free( TaggedHeap_s* const heap, TaggedAllocator_s* const alloc )
{
	std::lock_guard< std::mutex > lock( heap->mutex );

	heap->blockBitfield.xm = ::XMVectorXorInt( heap->blockBitfield.xm, alloc->tag.xm );
	alloc->tag = uint4_Zero;

	for ( auto& data : alloc->threadData )
		data = nullptr;

	for ( auto& heads : alloc->threadDataHead )
		heads = 0;
}

void TaggedAlloc_Init( TaggedAllocator_s* out_alloc )
{
	out_alloc->tag.xm = uint4_Zero.xm;
	memset( out_alloc->threadData, 0, sizeof( out_alloc->threadData ) );
	memset( out_alloc->threadDataHead, 0, sizeof( out_alloc->threadDataHead ) );
}

void* TaggedAlloc_Alloc( TaggedHeap_s* const heap, TaggedAllocator_s* alloc, u32 size, u32 index )
{
	Assert( size <= heap->blockSize );
	if ( alloc->threadData[index] == nullptr || alloc->threadDataHead[index] + size >= heap->blockSize )
	{
		if ( !TaggedHeap_Alloc( heap, alloc, index ) )
			return nullptr;
	}

	u8* const data = reinterpret_cast< u8* >( alloc->threadData[index] ) + alloc->threadDataHead[index];
	alloc->threadDataHead[index] += size;
	return data;
}

static void TaggedFenceHeap_PreAlloc( TaggedHeap_s* const heap )
{
	TaggedFenceHeap_s* const fenceHeap = static_cast< TaggedFenceHeap_s* >( heap );

	uint4 newBitfield = heap->blockBitfield;
	const u64 completedValue = fenceHeap->fence->GetCompletedValue();

	u64 tagIndex = fenceHeap->fenceTagStart;
	for ( ; tagIndex < fenceHeap->fenceTagEnd; ++tagIndex )
	{
		if ( fenceHeap->freedFenceValues[tagIndex] > completedValue )
			break;

		const TaggedHeapTag_t* const tag = &fenceHeap->freedFenceTags[tagIndex];
		newBitfield.xm = ::XMVectorXorInt( newBitfield.xm, tag->xm );
	}

	fenceHeap->fenceTagStart = tagIndex;
	fenceHeap->blockBitfield.xm = newBitfield.xm;
}

void TaggedFenceHeap_Init( const u32 blockSize, const u32 blockCount, void* const data, TaggedFenceHeap_s* const out_heap )
{
	TaggedHeap_Init( blockSize, blockCount, data, out_heap );

	out_heap->fenceTagStart = 0;
	out_heap->fenceTagEnd = 0;
	out_heap->preAlloc = &TaggedFenceHeap_PreAlloc;

	for ( u32 fence = 0; fence < TaggedHeap_TagsMaxCount; ++fence )
	{
		out_heap->freedFenceTags[fence].xm = uint4_Zero.xm;
		out_heap->freedFenceValues[fence] = ~0;
	}

	g_d3dDevice->CreateFence( 0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS( &out_heap->fence ) );
}

void TaggedFenceHeap_Free( TaggedFenceHeap_s* const heap, TaggedAllocator_s* alloc, const u64 fence )
{
	std::lock_guard< std::mutex > lock( heap->mutex );

	const u32 index = heap->fenceTagEnd % _countof( heap->freedFenceValues );
	heap->freedFenceTags[index] = alloc->tag;
	heap->freedFenceValues[index] = fence;
	++heap->fenceTagEnd;

	alloc->tag.xm = uint4_Zero.xm;

	for ( auto& data : alloc->threadData )
		data = nullptr;

	for ( auto& heads : alloc->threadDataHead )
		heads = 0;
}
