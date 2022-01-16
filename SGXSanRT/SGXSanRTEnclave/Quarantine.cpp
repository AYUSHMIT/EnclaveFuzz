#include <pthread.h>
#include <stdlib.h>
#include "Quarantine.hpp"
#include "SGXSanDefs.h"
#include "SGXSanPrintf.hpp"
#include "SGXSanManifest.h"
#include "SGXInternal.hpp"

#if (USE_SGXSAN_MALLOC)
#define BACKEND_FREE free
#else
#define BACKEND_FREE dlfree
#endif

static pthread_rwlock_t rwlock_quarantine_cache = PTHREAD_RWLOCK_INITIALIZER;

QuarantineCache quarantine_cache;
QuarantineCache *g_quarantine_cache = &quarantine_cache;

QuarantineCache::QuarantineCache()
{
    m_quarantine_cache_max_size = get_heap_size() / 0x400;
}

void QuarantineCache::put(QuarantineElement qe)
{
    uptr alignment = SHADOW_GRANULARITY;

#if 1
    // Consistency check
    pthread_rwlock_rdlock(&rwlock_quarantine_cache);
    if (m_queue.empty())
    {
        assert(m_quarantine_cache_used_size == 0);
    }
    pthread_rwlock_unlock(&rwlock_quarantine_cache);
#endif
    // if cache can not hold this element, directly free it
    if (qe.alloc_size > m_quarantine_cache_max_size)
    {
        BACKEND_FREE(reinterpret_cast<void *>(qe.alloc_beg));
        // PRINTF("[Recycle->Free] [0x%lx..0x%lx ~ 0x%lx..0x%lx)\n", qe.alloc_beg, qe.user_beg, qe.user_beg + qe.user_size, qe.alloc_beg + qe.alloc_size);
        FastPoisonShadow(qe.user_beg, RoundUpTo(qe.user_size, alignment), kAsanHeapLeftRedzoneMagic);
        return;
    }

    // pop queue util it can hold new element
    pthread_rwlock_wrlock(&rwlock_quarantine_cache);
    while (UNLIKELY((!m_queue.empty()) && (m_quarantine_cache_used_size + qe.alloc_size > m_quarantine_cache_max_size)))
    {
        QuarantineElement front_qe = m_queue.front();
        // free and poison
        BACKEND_FREE(reinterpret_cast<void *>(front_qe.alloc_beg));
        // PRINTF("[Quarantine->Free] [0x%lx..0x%lx ~ 0x%lx..0x%lx) \n", front_qe.alloc_beg, front_qe.user_beg, front_qe.user_beg + front_qe.user_size, front_qe.alloc_beg + front_qe.alloc_size);
        FastPoisonShadow(front_qe.user_beg, RoundUpTo(front_qe.user_size, alignment), kAsanHeapLeftRedzoneMagic);
        // update quarantine cache
        assert(m_quarantine_cache_used_size >= front_qe.alloc_size);
        m_quarantine_cache_used_size -= front_qe.alloc_size;
        m_queue.pop_front();
        if (m_queue.empty())
        {
            assert(m_quarantine_cache_used_size == 0);
        }
    }
    // PRINTF("[Recycle->Quaratine] [0x%lx..0x%lx ~ 0x%lx..0x%lx)\n", qe.alloc_beg, qe.user_beg, qe.user_beg + qe.user_size, qe.alloc_beg + qe.alloc_size);
    m_queue.push_back(qe);
    m_quarantine_cache_used_size += qe.alloc_size;
    pthread_rwlock_unlock(&rwlock_quarantine_cache);
}