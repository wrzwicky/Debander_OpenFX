#include "Processor.h"

// callback for ThreadSuite's multithreading function
void Processor::multiThreadProcessing(unsigned int threadId, unsigned int nThreads, void *arg)
{
	Processor *proc = (Processor *)arg;

	// slice the y range into the number of threads it has
	unsigned int dy = proc->window.y2 - proc->window.y1;

	unsigned int y1 = proc->window.y1 + threadId * dy / nThreads;
	unsigned int y2 = proc->window.y1 + Minimum((threadId + 1) * dy / nThreads, dy);

	OfxRectI win = proc->window;
	win.y1 = y1; win.y2 = y2;

	// and render that thread on each
	proc->doProcessing(win);
}

// function to kick off rendering across multiple CPUs
void
Processor::process(OfxMultiThreadSuiteV1 *pThreadSuite)
{
	unsigned int nThreads = 1;
//	pThreadSuite->multiThreadNumCPUs(&nThreads);
	pThreadSuite->multiThread(multiThreadProcessing, nThreads, (void *) this);
}
