#pragma once

#include "basictypes.h"


struct JobCounter_s;

struct JobDesc_s
{
	void (*task)( void* data );
	void* data;
};

struct JobScheduler_s
{
	void (*runJobs)( const JobDesc_s* jobs, u32 numJobs, JobCounter_s** counter );
	void (*wait)( JobCounter_s* counter, u32 value );
};

void JobScheduler_Init_ST();
void JobScheduler_Init_MT( u32 workerThreadCount );

extern JobScheduler_s jobApi;