#pragma once

#include <mutex>

#include "basictypes.h"
#include "constants.h"

static constexpr u64 TaggedHeap_TagsMaxCount = 128;
static constexpr u64 TaggedHeap_BitfieldIntCount = TaggedHeap_TagsMaxCount / 32;
static constexpr u64 TaggedHeap_BitfieldArraySize = TaggedHeap_BitfieldIntCount / 4;

using TaggedHeapTag_t = uint4;

struct TaggedHeap_s
{
	u64 blockSize;
	u64 blockCount;
	void* data;
	uint4 blockBitfield;

	std::mutex mutex;

	void (*preAlloc)( TaggedHeap_s* heap );
};

struct TaggedAllocator_s
{
	TaggedHeapTag_t tag;
	void* threadData[WORKER_THREADS_MAX_COUNT];
	u64 threadDataHead[WORKER_THREADS_MAX_COUNT];
};

void TaggedHeap_Init( u32 blockSize, u32 blockCount, void* data, TaggedHeap_s* out_heap );
bool TaggedHeap_Alloc( TaggedHeap_s* heap, TaggedAllocator_s* alloc, u32 index );
void TaggedHeap_Free( TaggedHeap_s* heap, TaggedAllocator_s* alloc );

void TaggedAlloc_Init( TaggedAllocator_s* out_alloc );
void* TaggedAlloc_Alloc( TaggedHeap_s* heap, TaggedAllocator_s* alloc, u32 size, u32 index );

struct TaggedFenceHeap_s : public TaggedHeap_s
{
	D3DPtr< ID3D12Fence > fence;
	u64 nextFence;

	TaggedHeapTag_t freedFenceTags[TaggedHeap_TagsMaxCount];
	u64 freedFenceValues[TaggedHeap_TagsMaxCount];

	u64 fenceTagStart;
	u64 fenceTagEnd;
};

void TaggedFenceHeap_Init( u32 blockSize, u32 blockCount, void* data, TaggedFenceHeap_s* out_heap );
void TaggedFenceHeap_Free( TaggedFenceHeap_s* heap, TaggedAllocator_s* alloc, u64 fence );

