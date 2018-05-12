#include "stdafx.h"
#include "job_system.h"

#include <atomic>

constexpr u32 DEFAULT_FIBER_STACK_SIZE = 64 * 1024;
//constexpr u32 LARGE_FIBER_STACK_SIZE = 512 * 1024;

constexpr u32 DEFAULT_FIBER_POOL_SIZE = 128;
static void* s_fiberPool[DEFAULT_FIBER_POOL_SIZE];
static u32 s_fiberFreeList[DEFAULT_FIBER_POOL_SIZE];
static u32 s_fiberFreeListSize = DEFAULT_FIBER_POOL_SIZE;

static struct WaitFiber_s
{
	u32 fiberId;
	u32 value;
	u32 counterIndex;
	u16 prev;
	u16 next;
} s_fiberWaitQueue[DEFAULT_FIBER_POOL_SIZE];
static u16 s_fiberWaitQueueHead = 0;
static u16 s_fiberWaitQueueTail = 0;

constexpr u32 JOB_QUEUE_MAX_COUNT = 256;
static JobDesc_s s_jobQueue[JOB_QUEUE_MAX_COUNT];
static u32 s_jobCounterReferences[JOB_QUEUE_MAX_COUNT];
static u32 s_jobQueueHead = 0;
static u32 s_jobQueueTail = 0;

struct JobCounter_s
{
	std::atomic_uint32_t value;
};

constexpr u32 JOB_COUNTER_MAX_COUNT = 64;
static JobCounter_s s_jobCounters[JOB_COUNTER_MAX_COUNT];
static u32 s_counterFreeList[JOB_COUNTER_MAX_COUNT];
static u32 s_counterFreeListSize = JOB_COUNTER_MAX_COUNT;

static void JobScheduler_Init_ST( const JobDesc_s* jobs, const u32 numJobs, JobCounter_s** counter );
static void JobScheduler_RunJobs_ST( const JobDesc_s* jobs, const u32 numJobs, JobCounter_s** counter );
static void JobScheduler_Wait_ST( JobCounter_s* counter, u32 value );

JobScheduler_s jobApi
{
	&JobScheduler_Init_ST,
	&JobScheduler_Wait_ST
};

struct FiberData_s
{
	u32 id;
};

static void* s_mainFiber_ST;
static void Fiber_WorkProc( void* fiberDataPtrCast )
{
	const FiberData_s fiberData{ reinterpret_cast< u32 >( fiberDataPtrCast ) };

	while ( true )
	{
		JobDesc_s job = s_jobQueue[s_jobQueueHead];
		JobCounter_s* counter = &s_jobCounters[s_jobCounterReferences[s_jobQueueHead]];
		++s_jobQueueHead;

		job.task( job.data );
		--counter->value;

		s_fiberFreeList[s_fiberFreeListSize] = fiberData.id;
		++s_fiberFreeListSize;

		SwitchToFiber( s_mainFiber_ST );
	}
}

static void CreateFiberPools( void( *fiberFunc )(void*) )
{
	for ( u32 i = 0; i < DEFAULT_FIBER_POOL_SIZE; ++i )
	{
		static_assert( sizeof( FiberData_s ) <= sizeof( void* ), "" );
		s_fiberPool[i] = CreateFiberEx( DEFAULT_FIBER_STACK_SIZE, 0, 0, fiberFunc, reinterpret_cast< void* >( i ) );
		s_fiberFreeList[i] = i;
	}

	for ( u32 i = 0; i < JOB_QUEUE_MAX_COUNT; ++i )
	{
		s_fiberWaitQueue[i].prev = i-1;
		s_fiberWaitQueue[i].next = i+1;
	}
}

static void JobScheduler_Init_ST( const JobDesc_s* jobs, const u32 numJobs, JobCounter_s** counter )
{
	jobApi.runJobs = &JobScheduler_RunJobs_ST; 

	CreateFiberPools( &Fiber_WorkProc );

	jobApi.runJobs( jobs, numJobs, counter );
}

static void JobScheduler_RunJobs_ST( const JobDesc_s* jobs, const u32 numJobs, JobCounter_s** counter )
{
	Assert( s_jobQueueTail - s_jobQueueHead + JOB_QUEUE_MAX_COUNT >= numJobs && "Job scheduler job queue full." );

	const u32 counterIndex = s_counterFreeList[s_counterFreeListSize-1];
	--s_counterFreeListSize;

	*counter = &s_jobCounters[counterIndex];
	(*counter)->value = numJobs;

	const u32 tailIndex = s_jobQueueTail % JOB_QUEUE_MAX_COUNT;
	const u32 tailJobs = min( numJobs, JOB_QUEUE_MAX_COUNT - tailIndex );
	memcpy( &s_jobQueue[tailIndex], jobs, sizeof( jobs[0] ) * tailJobs );
	memset( &s_jobCounterReferences[tailIndex], counterIndex, sizeof( s_jobCounterReferences[0] ) * tailJobs );

	const u32 remainingJobs = numJobs - tailJobs;
	memcpy( &s_jobQueue[0], jobs, sizeof( jobs[0] ) * remainingJobs );
	memset( &s_jobCounterReferences[0], counterIndex, sizeof( s_jobCounterReferences[0] ) * remainingJobs );

	s_jobQueueTail += numJobs;
	s_mainFiber_ST = ConvertThreadToFiberEx( nullptr, 0 );

	while ( s_jobQueueTail > s_jobQueueHead || s_fiberWaitQueueHead != s_fiberWaitQueueTail)
	{
		void* nextFiber = nullptr;

		for ( u16 i = s_fiberWaitQueueHead; i != s_fiberWaitQueueTail; i = s_fiberWaitQueue[i].next )
		{
			WaitFiber_s* waitFiber = &s_fiberWaitQueue[i];
			const JobCounter_s* const counter = &s_jobCounters[waitFiber->counterIndex];
			if ( counter->value <= waitFiber->value )
			{
				nextFiber = s_fiberPool[waitFiber->fiberId];
				
				if ( waitFiber->next == s_fiberWaitQueueTail )
					s_fiberWaitQueueTail = i;
				else if ( i == s_fiberWaitQueueHead )
					s_fiberWaitQueueHead = waitFiber->next;
				else
				{
					s_fiberWaitQueue[waitFiber->prev].next = waitFiber->next;
					s_fiberWaitQueue[waitFiber->next].prev = waitFiber->prev;
				}

				break;
			}
		}

		if ( !nextFiber )
		{
			Assert( s_fiberFreeListSize > 0 && "Fiber deadlock" );
			const u32 freeFiberIndex = s_fiberFreeList[s_fiberFreeListSize - 1];
			--s_fiberFreeListSize;
			nextFiber = s_fiberPool[freeFiberIndex];
		}

		Assert( nextFiber );
		SwitchToFiber( nextFiber );
	}

	ConvertFiberToThread();
}

static void JobScheduler_Wait_ST( JobCounter_s* counter, u32 value )
{
	FiberData_s fiberData{ reinterpret_cast<u32>( GetFiberData() ) };

	const u16 waitFiberIndex = s_fiberWaitQueueTail;
	s_fiberWaitQueueTail = s_fiberWaitQueue[waitFiberIndex].next;

	const ptrdiff_t counterIndex = counter - s_jobCounters;
	Assert( counterIndex >= 0 && counterIndex < JOB_COUNTER_MAX_COUNT );
	s_fiberWaitQueue[waitFiberIndex].counterIndex = counterIndex;
	s_fiberWaitQueue[waitFiberIndex].fiberId = fiberData.id;
	s_fiberWaitQueue[waitFiberIndex].value = value;

	SwitchToFiber( s_mainFiber_ST );
}

static u32 WorkerThreadCount = 0;
static thread_local u32 WorkerThreadIndex = (u32)-1;
void JobScheduler_Init_MT( u32 workerThreadCount )
{

}
