#pragma once

/**
 * @brief Spinning mutex that uses atomics.
 */
class FAtomicReadWriteLock
{
	// It follows UE Standards & is recommended to use std::atomic vs UEs TAtomic, as those have been deprecated, as well
	// as std::atomic being a guaranteed cross-platform subset of the STL.
	// Sources:
	// https://docs.unrealengine.com/5.1/en-US/epic-cplusplus-coding-standard-for-unreal-engine/
	// https://docs.unrealengine.com/5.0/en-US/API/Runtime/Core/Templates/TAtomic/
private:
	static constexpr int32 LockedState = -1;
	static constexpr int32 UnlockedState = 0;

	std::atomic<int32> LockObject = UnlockedState;
	
public:

	/**
	 * @brief Spins thread while waiting for to acquire a lock. 
	 */
	void SpinWaitWriteLock();
	
	void SpinWaitReadLock();

	/**
	 * @brief Unlocks the mutex immediately.
	 */
	void WriteUnlock();

	void ReadUnlock();
};

FORCEINLINE void FAtomicReadWriteLock::SpinWaitWriteLock()
{
	int32 CurrentState = UnlockedState;

	while(!LockObject.compare_exchange_weak(CurrentState, LockedState))
	{
		// Reset current state
		CurrentState = UnlockedState;
	}
}

FORCEINLINE  void FAtomicReadWriteLock::SpinWaitReadLock()
{
	int32 CurrentState = LockObject;

	do
	{
		if(LockObject == LockedState)
			continue;

		if(LockObject.compare_exchange_weak(CurrentState, CurrentState+1))
			break;
	}while(true);
}

FORCEINLINE void FAtomicReadWriteLock::WriteUnlock()
{
	// LockObject.store(UnlockedState);
	int32 CurrentState = LockedState;

	while(!LockObject.compare_exchange_weak(CurrentState, UnlockedState))
	{
		// Reset current state
		CurrentState = LockedState;
	}
}

FORCEINLINE void FAtomicReadWriteLock::ReadUnlock()
{
	int32 CurrentState = LockObject;

	while(!LockObject.compare_exchange_weak(CurrentState, CurrentState-1))
	{
	}
}
