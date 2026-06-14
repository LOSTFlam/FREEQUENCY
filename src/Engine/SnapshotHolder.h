#pragma once

#include <juce_core/juce_core.h>

namespace freequency::engine
{
    /**
        SnapshotHolder<T> — lock-free hand-off of immutable, reference-counted
        snapshots from the message thread to the audio thread.

        `T` must derive from juce::ReferenceCountedObject and expose
        `using Ptr = juce::ReferenceCountedObjectPtr<T>;`.

        Design (the "publish an immutable copy + message-thread GC" pattern)
        -------------------------------------------------------------------
        - publish() (message thread): swaps in the new snapshot under a brief
          spin-lock and *retains the retired one* in a garbage list so its
          destructor never runs on the audio thread.
        - getForAudio() (audio thread): try-locks; on success refreshes its
          audio-thread-only cached pointer, otherwise keeps using the previous
          one. It returns a Ptr copy — only an atomic refcount bump, no alloc.
          Because the message thread always keeps the live snapshot referenced
          (in `active` or `garbage`), the audio thread's refcount drops can never
          reach zero, so deletion only ever happens on the message thread.
        - collectGarbage() (message thread, e.g. from a timer): frees retired
          snapshots once nothing else references them.

        The spin-lock is held only for a pointer assignment; no allocation or
        heavy work ever happens inside it, so the audio thread's try-lock
        essentially never contends.
    */
    template <typename T>
    class SnapshotHolder
    {
    public:
        using Ptr = typename T::Ptr;

        SnapshotHolder() = default;

        /** Message thread: make `newSnapshot` the current one. */
        void publish (Ptr newSnapshot)
        {
            const juce::SpinLock::ScopedLockType l (lock);

            if (active != nullptr)
                garbage.add (active);

            active = std::move (newSnapshot);
        }

        /** Audio thread: get the snapshot to use for this block (may be null). */
        Ptr getForAudio() noexcept
        {
            const juce::SpinLock::ScopedTryLockType l (lock);

            if (l.isLocked())
                audioCached = active;

            return audioCached;
        }

        /** Message thread: release retired snapshots no longer in use. */
        void collectGarbage()
        {
            const juce::SpinLock::ScopedLockType l (lock);

            for (int i = garbage.size(); --i >= 0;)
                if (garbage.getObjectPointer (i)->getReferenceCount() == 1)
                    garbage.remove (i);
        }

        /** Message thread: drop everything (e.g. on graph teardown). */
        void clear()
        {
            const juce::SpinLock::ScopedLockType l (lock);
            active = nullptr;
            garbage.clear();
        }

    private:
        juce::SpinLock lock;
        Ptr active;                                   // current (guarded)
        Ptr audioCached;                              // audio-thread-only cache
        juce::ReferenceCountedArray<T> garbage;       // retired, awaiting GC

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SnapshotHolder)
    };
} // namespace freequency::engine
