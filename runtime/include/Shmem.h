// shared memory hooks

#ifndef NEWCOMAIR_RUNTIME_SHMEM_H
#define NEWCOMAIR_RUNTIME_SHMEM_H

/**
 * Open a shared memory to store results, provide a ptr->buffer to operate on.
 */
void InitMemHooks();

/**
 * Truncate the shared memory buffer to the actual data size, then close.
 */
void FinalizeMemHooks();

/**
 * Write the begin address and length of the accessed memory to shared memory buffer.
 * @param beginAddress the begin address of the accessed memory
 * @param length the length of the accessed memory
 */
void RecordMemHooks(void *beginAddress, unsigned long length);

/**
 * Write the exec times of the cloned Loop to shared memory buffer.
 * @param cost the exec times of the cloned Loop
 *
 * The first element is set to zero to distinguish from the RecordMemHook output.
 */
void RecordCostHooks(unsigned long cost);


#endif //NEWCOMAIR_RUNTIME_SHMEM_H
