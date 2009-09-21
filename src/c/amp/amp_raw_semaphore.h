/*
 * Copyright (c) 2009, Bjoern Knafla
 * http://www.bjoernknafla.com/
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are 
 * met:
 *
 *   * Redistributions of source code must retain the above copyright 
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright 
 *     notice, this list of conditions and the following disclaimer in the 
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Bjoern Knafla 
 *     Parallelization + AI + Gamedev Consulting nor the names of its 
 *     contributors may be used to endorse or promote products derived from 
 *     this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 *
 * Shallow wrapper around a semaphore type of the platform which is only
 * used inside one process.
 *
 * @attention Don't copy or move semaphores - copying and moving pointers to
 *            them is ok though you need to take care about ownership 
 *            management.
 * 
 * @attention Don't pass pointers to an invalid semaphore structure to any
 *            of the functions. Don't pass non-initialized (or after 
 *            initialization destroyed) semaphores to any function other than
 *            to amp_raw_semaphore_init. Don't pass initialized semaphores
 *            to amp_raw_semaphore_init.
 *
 * @attention ENOSYS might be returned by the functions if the POSIX 1003 1b 
 *            backend is used and the system doesn't support semaphores. Use 
 *            another backend while compiling, for example AMP_USE_WINTHREADS,
 *            or AMP_USE_PTHREADS and don't define
 *            AMP_USE_POSIX_1003_1b_SEMAPHORES.
 *
 * TODO: @todo Add a state variable to amp_raw_semaphore_s to help 
 *             detecting use of an uninitialized instance.
 *
 * TODO: @todo Document return codes for all supported platforms.
 *
 * TODO: @todo When adding a trywait function look if POSIX specifies EBUSY
 *             or EAGAIN as a return value to indicate that the thread would
 *             block.
 */

#ifndef AMP_amp_raw_semaphore_H
#define AMP_amp_raw_semaphore_H



#if defined(AMP_USE_POSIX_1003_1B_SEMAPHORES)
#   include <semaphore.h>
#elif defined(AMP_USE_PTHREADS)
#   include <pthread.h>
#   include <limits.h>
#elif defined(AMP_USE_WINTHREADS)
#   define WIN32_LEAN_AND_MEAN
#   include <windows.h>
#   include <limits.h>
#else
#   error Unsupported platform.
#endif




#if defined(__cplusplus)
extern "C" {
#endif
    
    
    /**
     * Type used to specify the semaphore start count when initialized.
     * 
     * When changing the type also adapt AMP_RAW_SEMAPHORE_COUNT_MAX
     * and check if limits.h must be included or not.
     */
#if defined(AMP_USE_POSIX_1003_1B_SEMAPHORES)
    typedef unsigned int amp_raw_semaphore_count_t;
#elif defined(AMP_USE_PTHREADS)
    typedef unsigned int amp_raw_semaphore_count_t;
#elif defined(AMP_USE_WINTHREADS)
    typedef long amp_raw_semaphore_count_t;
#else
#   error Unsupported platform.
#endif

    /**
     * @def  AMP_RAW_SEMAPHORE_COUNT_MAX
     *
     * Maximal value of the internal semaphore counter to not-block threads
     * when waiting on the semaphore.
     *
     * TODO: @todo Try to find a Windows platform constant for the max number
     *             of semaphores allowed (not found yet).
     */
#if defined(AMP_USE_POSIX_1003_1B_SEMAPHORES)
#   define AMP_RAW_SEMAPHORE_COUNT_MAX ((amp_raw_semaphore_count_t)(SEM_VALUE_MAX))
#elif defined(AMP_USE_PTHREADS)
#   define AMP_RAW_SEMAPHORE_COUNT_MAX ((amp_raw_semaphore_count_t)(UINT_MAX))
#elif defined(AMP_USE_WINTHREADS)
#   define AMP_RAW_SEMAPHORE_COUNT_MAX ((amp_raw_semaphore_count_t)(LONG_MAX))
#else
#   error Unsupported platform.
#endif
    
    
    /**
     * Must be initialized before usage and finalized to free reserved 
     * resources.
     *
     * @attention Don't copy or move, otherwise behavior is undefined.
     */
    struct amp_raw_semaphore_s {
#if defined(AMP_USE_POSIX_1003_1B_SEMAPHORES)
        sem_t semaphore;
#elif defined(AMP_USE_PTHREADS)
        pthread_mutex_t mutex;
        pthread_cond_t a_thread_can_pass;
        amp_raw_semaphore_count_t count;
#elif defined(AMP_USE_WINTHREADS)
        HANDLE semaphore_handle;
#else
#   error Unsupported platform.
#endif
    };
    typedef struct amp_raw_semaphore_s *amp_raw_semaphore_t;
    
    
    /**
     * Initializes the semaphore pointed to by sem.
     *
     * @param init_count specifies the initial (non-negative and lesser or equal
     *                   than AMP_RAW_SEMAPHORE_COUNT_MAX) semaphore counter 
     *                   value.
     *
     * @return AMP_SUCCESS on successful initialization, otherwise:
     *         ENOMEM if memory is insufficient.
     *         EAGAIN if other system resources are insufficient.
     *         ENOSPC if the POSIX 1003 1b backend is used and the system lacks
     *         resources.
     *         ENOSYS if the POSIX 1003 1b backend is used and the system 
     *         doesn't support semaphores. Use another backend while compiling,
     *         for example AMP_USE_WINTHREADS, or AMP_USE_PTHREADS and don't 
     *         define AMP_USE_POSIX_1003_1b_SEMAPHORES.
     *         Other error codes might be returned to signal errors while
     *         initializing, too. These are programming errors and mustn't 
     *         occur in release code. When @em amp is compiled without NDEBUG
     *         set it might assert that these programming errors don't happen.
     *         EINVAL if the semaphore is invalid, the init_count is negative or
     *         greater than AMP_RAW_SEMAPHORE_COUNT_MAX.
     *         EPERM if the process lacks privileges to initialize the 
     *         semaphore.
     *         EBUSY if the semaphore is already initialized.
     *
     * @attention sem mustn't be NULL.
     *
     * @attention Don't pass an initialized (and not finalized after 
     *            initialization) semaphore to amp_raw_semaphore_init.
     *
     * @attention init_count mustn't be negative and mustn't be greater than
     *            AMP_RAW_SEMAPHORE_COUNT_MAX.
     *
     * TODO: @todo See how many of the backend specific error codes are really
     *             needed.
     */
    int amp_raw_semaphore_init(struct amp_raw_semaphore_s *sem,
                               amp_raw_semaphore_count_t init_count);
    
    /**
     * Finalizes a semaphore and frees the resources it used.
     *
     * @return AMP_SUCCESS on successful finalization.
     *         ENOSYS if the backend doesn't support semaphores.
     *         Error codes might be returned to signal errors while
     *         finalization, too. These are programming errors and mustn't 
     *         occur in release code. When @em amp is compiled without NDEBUG
     *         set it might assert that these programming errors don't happen.
     *         EBUSY if threads block on the semaphore.
     *         EINVAL if the semaphore isn't valid, e.g. not initialized.
     *
     * @attention sem mustn't be NULL.
     *
     * @attention Don't pass an uninitialized semaphore into 
     *            amp_raw_semaphore_finalize.
     *
     * @attention Don't call on a blocked semaphore, otherwise behavior is 
     *            undefined.
     */
    int amp_raw_semaphore_finalize(struct amp_raw_semaphore_s *sem);
    
    /**
     * If the semaphore counter is not zero decrements the counter and pass the
     * semaphore. If the counter is zero the thread blocks until the semaphore
     * counter becomes greater than zero again and its the threads turn to 
     * decrease and pass it.
     *
     * @return AMP_SUCCESS after waited successful on the semaphore.
     *         EINTR if the semaphore was interrupted by a signal when using a
     *         backend that supports signal interruption.
     *         ENOSYS if the backend doesn't support semaphores.
     *         EOVERFLOW if the semaphore counter value exceeds 
     *         Error codes might be returned to signal errors while
     *         waiting, too. These are programming errors and mustn't 
     *         occur in release code. When @em amp is compiled without NDEBUG
     *         set it might assert that these programming errors don't happen.
     *         EDEADLK if a deadlock condition was detected.
     *         EINVAL if the semaphore isn't valid, e.g. not initialized.
     *         EPERM if the process lacks privileges to wait on the 
     *         semaphore.
     *
     * @attention sem mustn't be NULL.
     *
     * @attention Based on the backend amp_raw_semaphores might or might not 
     *            react to / are or are not usable with signals. Set the threads
     *            signal mask to not let any signals through.
     *
     * TODO: @todo Decide if os signals should be able to interrupt the waiting.
     */
    int amp_raw_semaphore_wait(struct amp_raw_semaphore_s *sem);
    
    /**
     * Increments the semaphore counter by one and if threads are blocked on the
     * semaphore one of them is woken up and gets the chance to decrease the 
     * counter and pass the semaphore.
     *
     * @return AMP_SUCCESS after succesful signaling the semaphore.
     *         ENOSYS if the backend doesn't support semaphores.
     *         EOVERFLOW or EAGAIN if the semaphore counter value exceeds 
     *           AMP_RAW_SEMAPHORE_COUNT_MAX .
     *         Error codes might be returned to signal errors while
     *         signaling, too. These are programming errors and mustn't 
     *         occur in release code. When @em amp is compiled without NDEBUG
     *         set it might assert that these programming errors don't happen.
     *         EINVAL if the semaphore isn't valid, e.g. not initialized.
     *         EDEADLK if a deadlock condition is detected.
     *         EPERM if the process lacks privileges to signal the 
     *         semaphore.
     *
     * @attention sem mustn't be NULL.
     */
    int amp_raw_semaphore_signal(struct amp_raw_semaphore_s *sem);
    
    
    
    
#if defined(__cplusplus)
} /* extern "C" */
#endif
    

#endif /* AMP_amp_raw_semaphore_H */