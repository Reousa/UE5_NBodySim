#pragma once
#include "FAtomicReadWriteLock.h"

/**
 * @brief Scoped lock that uses FAtomicMutex
 */
class FAtomicScopeLock
{
private:
	FAtomicReadWriteLock& Mutex;
	
public:
	FORCEINLINE explicit FAtomicScopeLock(FAtomicReadWriteLock& InMutex) :
	Mutex(InMutex)
	{
		Mutex.SpinWaitWriteLock();
	}

	FORCEINLINE ~FAtomicScopeLock() noexcept
	{
		Mutex.WriteUnlock();
	}
};
