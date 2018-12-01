// shared memory hooks

#ifndef NEWCOMAIR_SHMEM_H
#define NEWCOMAIR_SHMEM_H

/**
 * Open a shared memory to store results, provide a ptr->buffer to operate on.
 */
void InitMemHooks();

/**
 * Truncate the shared memory buffer to the actual data size, then close.
 */
void FinalizeMemHooks();

#endif //NEWCOMAIR_SHMEM_H
