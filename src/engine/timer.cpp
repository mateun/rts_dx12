#include "timer.h"

Timer::Timer()
{
    QueryPerformanceFrequency(&frequency);
}

void Timer::start()
{
    QueryPerformanceCounter(&currentCounter);  
}

void Timer::stop()
{
    QueryPerformanceCounter(&stopCounter);
}

float Timer::diffInSeconds()
{
    return (stopCounter.QuadPart - currentCounter.QuadPart) / (float) frequency.QuadPart;
}
