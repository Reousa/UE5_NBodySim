#pragma once
#include "FAtomicMutex.h"

/**
 * @brief Scoped lock that uses FAtomicMutex
 */
class FAtomicScopeLock
{
private:
	FAtomicMutex& Mutex;
	
public:
	FORCEINLINE explicit FAtomicScopeLock(FAtomicMutex& InMutex) noexcept :
	Mutex(InMutex)
	{
		Mutex.SpinWaitLock();
	}

	FORCEINLINE ~FAtomicScopeLock() noexcept
	{
		Mutex.Unlock();
	}
};
