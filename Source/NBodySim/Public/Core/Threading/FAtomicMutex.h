#pragma once

/**
 * @brief Spinning mutex that uses atomics.
 */
class FAtomicMutex
{
	// It follows UE Standards & is recommended to use std::atomic vs UEs TAtomic, as those have been deprecated, as well
	// as std::atomic being a guaranteed cross-platform subset of the STL.
	// Sources:
	// https://docs.unrealengine.com/5.1/en-US/epic-cplusplus-coding-standard-for-unreal-engine/
	// https://docs.unrealengine.com/5.0/en-US/API/Runtime/Core/Templates/TAtomic/
private:
	static constexpr bool LockedState = true;
	static constexpr bool UnlockedState = false;

	std::atomic<bool> LockObject;
	
public:

	/**
	 * @brief Spins thread while waiting for to acquire a lock. 
	 */
	FORCEINLINE void SpinWaitLock()
	{
		bool CurrentState = UnlockedState;

		while(!LockObject.compare_exchange_weak(CurrentState, true, std::memory_order_relaxed))
		{
			// Reset current state
			CurrentState = UnlockedState;
		}
	}

	/**
	 * @brief Unlocks the mutex immediately.
	 */
	FORCEINLINE void Unlock()
	{
		// Less safe then using a CAS loop, should be used with a scope lock.
		check(LockObject == LockedState);
		LockObject.store(UnlockedState, std::memory_order_relaxed);
	}
};
