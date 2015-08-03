#include "stdafx.h"
#include "ChunkAccessor.h"

#include "ChunkAllocator.h"

const ui32 HANDLE_STATE_DEAD = 0;
const ui32 HANDLE_STATE_BIRTHING = 1;
const ui32 HANDLE_STATE_ALIVE = 2;
const ui32 HANDLE_STATE_ZOMBIE = 3;

ChunkHandle ChunkHandle::acquire() {
    return m_chunk->accessor->acquire(*this);
}
void ChunkHandle::release() {
    m_chunk->accessor->release(*this);
}

void ChunkAccessor::init(PagedChunkAllocator* allocator) {
    m_allocator = allocator;
}
void ChunkAccessor::destroy() {
    std::unordered_map<ChunkID, ChunkHandle>().swap(m_chunkLookup);
}

ChunkHandle ChunkAccessor::acquire(ChunkID id) {
    // TODO(Cristian): Try to not lock down everything
    bool wasOld;
    ChunkHandle chunk = safeAdd(id, wasOld);
    return wasOld ? acquire(chunk) : chunk;
}
ChunkHandle ChunkAccessor::acquire(ChunkHandle& chunk) {
    // Skip this for now
    goto SKIP;

    // If it's dead, we want to make it come to life
    switch (InterlockedCompareExchange(&chunk->m_handleState, HANDLE_STATE_BIRTHING, HANDLE_STATE_DEAD)) {
    case HANDLE_STATE_BIRTHING:
        // We can safely increment here
        InterlockedIncrement(&chunk->m_handleRefCount);
        // Make this alive
        InterlockedCompareExchange(&chunk->m_handleState, HANDLE_STATE_ALIVE, HANDLE_STATE_BIRTHING);
        break;
    case HANDLE_STATE_ALIVE:
    { // Make sure that this is kept alive
        ui32 state = chunk->m_handleState;
        while ((state = InterlockedCompareExchange(&chunk->m_handleState, HANDLE_STATE_BIRTHING, HANDLE_STATE_ALIVE)) != HANDLE_STATE_BIRTHING) {
            // Push. Puuuuusssshhh. PUUUUSHHHHH!!!!!!!
            continue;
        }
    }
    case HANDLE_STATE_ZOMBIE:
        // Someone is trying to kill this chunk, don't let them
    case HANDLE_STATE_DEAD:
        // It's dead, it must be revived
    default:
        break;
    }

SKIP:
    InterlockedIncrement(&chunk->m_handleRefCount);
    return chunk;
}

void ChunkAccessor::release(ChunkHandle& chunk) {
    // TODO(Cristian): Heavy thread-safety
    ui32 currentCount = InterlockedDecrement(&chunk->m_handleRefCount);
    if (currentCount == 0) {
        // If the chunk is alive, set it to zombie mode. Otherwise, it's being birthed.
        if (InterlockedCompareExchange(&chunk->m_handleState, HANDLE_STATE_ZOMBIE, HANDLE_STATE_ALIVE) == HANDLE_STATE_ZOMBIE) {
            // Now that it's a zombie, we kill it.
            if (InterlockedCompareExchange(&chunk->m_handleState, HANDLE_STATE_DEAD, HANDLE_STATE_ZOMBIE) == HANDLE_STATE_DEAD) {
                safeRemove(chunk->m_id);
            }
        }
        // TODO(Cristian): This needs to be added to a free-list


        // Make sure it can't be accessed until acquired again
        chunk->accessor = nullptr;
        m_chunkLookup.erase(chunk->m_id);
        // TODO(Ben): Time based free?
        m_allocator->free(chunk.m_chunk);
    }
    chunk.m_chunk = nullptr;
}

ChunkHandle ChunkAccessor::safeAdd(ChunkID id, bool& wasOld) {
    std::unique_lock<std::mutex> l(m_lckLookup);
    auto& it = m_chunkLookup.find(id);
    if (it == m_chunkLookup.end()) {
        wasOld = false;
        ChunkHandle& rv = m_chunkLookup[id];
        rv.m_chunk = m_allocator->alloc();
        rv->m_id = id;
        rv->accessor = this;
        rv->m_handleState = HANDLE_STATE_ALIVE;
        rv->m_handleRefCount = 1;
        return rv;
    } else {
        wasOld = true;
        return it->second;
    }
}

void ChunkAccessor::safeRemove(ChunkID id) {
    std::unique_lock<std::mutex> l(m_lckLookup);
    m_chunkLookup.erase(id);
}
