/*
 * Copyright (c) 2009-2010, Bjoern Knafla
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
 * Implementation of POSIX thread condition variable alikes using Windows
 * CRITICAL_SECTION, events, event waiting and signaling, and semaphores.
 *
 * See http://www.cse.wustl.edu/~schmidt/win32-cv-1.html for infos about
 * possible different implementation techniques but beware - there are bugs
 * in all so called "solutions" shown.
 *
 * See http://www.opengroup.org/onlinepubs/000095399/functions/pthread_cond_timedwait.html
 * for more infos about the POSIX threads condition variable specification.
 *
 * Many thanks to Anthony Williams and Dimitriy V'jukov (names in order of 
 * discussion contacts) for their interest and time to discuss the
 * condition variable implementation, and their invaluable and awesome feedback
 * that opened my eyes and made this code more correct and faster. All remaining
 * errors in the code are mine.
 * 
 * TODO: @todo Check if SEH (Windows structured exception handling) should be
 *             added.
 *
 * TODO: @todo Exchange the inner mutex against a critical section again.
 */


#include "amp_condition_variable.h"

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stddef.h>

#include "amp_stddef.h"
#include "amp_internal_winthreads_critical_section_config.h"
#include "amp_mutex.h"
#include "amp_raw_mutex.h"
#include "amp_raw_condition_variable.h"



int amp_raw_condition_variable_init(amp_condition_variable_t cond)
{
    assert(NULL != cond);
    
    if (NULL == cond) {
        return EINVAL;
    }
    
    

    
    int retval = InitializeCriticalSectionAndSpinCount(&cond->access_waiting_threads_count_critsec,
                                                   AMP_RAW_MUTEX_WINTHREADS_CRITICAL_SECTION_DEFAULT_SPIN_COUNT | AMP_RAW_MUTEX_WINTHREADS_CRITICAL_SECTION_CREATE_IMMEDIATELY_ON_WIN2000);
    
    if (FALSE == retval) {
        
        
        return ENOMEM;
    }
    

    
    retval = InitializeCriticalSectionAndSpinCount(&cond->wake_waiting_threads_critsec,
                                                   AMP_RAW_MUTEX_WINTHREADS_CRITICAL_SECTION_DEFAULT_SPIN_COUNT | AMP_RAW_MUTEX_WINTHREADS_CRITICAL_SECTION_CREATE_IMMEDIATELY_ON_WIN2000);
    if (FALSE  == retval) {
        /* GetLastError has more infos. */
        DeleteCriticalSection(&cond->access_waiting_threads_count_critsec);
        
        return EAGAIN;
    }
    
    
    
    
    /* Assuming that less threads exist than max possible semaphore count.
     */
    cond->waking_waiting_threads_count_control_sem = CreateSemaphore(NULL, /* No inheritance to child processes */
                                                                     0, /* Initially no threads can pass */
                                                                     LONG_MAX, /* Max semaphore count */
                                                                     NULL); /* Only intra-process semaphore */
    
    if (NULL == cond->waking_waiting_threads_count_control_sem) {
        DWORD const last_error = GetLastError();
        
        DeleteCriticalSection(&cond->wake_waiting_threads_critsec);
        DeleteCriticalSection(&cond->access_waiting_threads_count_critsec);
        
        int ret_error_code = AMP_SUCCESS;
        
        switch (last_error) {
            case ERROR_TOO_MANY_SEMAPHORES:
                ret_error_code = EAGAIN;
                break;
            default:
                /* TODO: @todo Check which code to use to flag an unknown error. 
                 */
                assert(false && "Unknown error.");
                ret_error_code = EINVAL;
                break;
        }
        
        return ret_error_code;
    }
    
    
    
    
    
    cond->finished_waking_waiting_threads_event = CreateEvent(NULL, /* Default security and no inheritance to child processes */
                                                              FALSE, /* No manual reset */
                                                              0, /* Initially not signaled */
                                                              NULL /* Not inter-process available */
                                                              );
    
    if (NULL == cond->finished_waking_waiting_threads_event) {
        
        DWORD const create_event_error = GetLastError();
        
        DeleteCriticalSection(&cond->wake_waiting_threads_critsec);
        DeleteCriticalSection(&cond->access_waiting_threads_count_critsec);

        BOOL const close_retval = CloseHandle(&cond->waking_waiting_threads_count_control_sem);
        int ret_error_code = AMP_SUCCESS;
        
        if (FALSE == close_retval) {
            DWORD const close_handle_error = GetLastError();
            
            switch (close_handle_error) {
                case ERROR_SEM_IS_SET:
                    /* TODO: @todo Need a Windows dev expert to check if this is the 
                     * right error code interpretation. 
                     */
                    assert(false && "Semaphore is in use.");
                    
                    ret_error_code = EBUSY;
                    break;
                case ERROR_SEM_OWNER_DIED:
                    /* TODO: @todo Check if this error can really happen. */
                    assert(false 
                           && "It shouldn't happen that the previous ownership of the semaphore has ended.");
                    ret_error_code = EINVAL;
                    break;
                case ERROR_SEM_NOT_FOUND:
                    assert(false && "It shouldn't happen that the (not) specified system semaphore name wasn't found.");
                    ret_error_code = EINVAL;
                    break;
                default:
                    /* TODO: @todo Check which code to use to flag an unknown error. 
                     */
                    assert(false && "Unknown error.");
                    ret_error_code = EINVAL;
                    break;
            }
            
            return ret_error_code;
        }
        
        /* I don't know the possible return values of GetLastError if event
         * creation didn't work - just returning an error code.
         */
        return EAGAIN;
    }
    
    
    cond->waiting_thread_count = 0l;
    cond->broadcast_in_progress = FALSE;
    
    
    /* Preliminary tests that waiting_thread_count and broadcast_in_progress
     * are correctly aligned to allow atomic access to them.
     *
     * TODO: @todo Re-enable alignment test assertions.
     *
     * TODO: @todo Check which alignment is needed on 32bit and 64bit systems
     *             and on which platforms.
     */
    /* assert(0x0 == ((uintptr_t)(&cond->waiting_thread_count) & 0x3)); */
    /* assert(0x0 ==((uintptr_t)(&cond->broadcast_in_progress) & 0x3)); */
    
    return AMP_SUCCESS;
}



int amp_raw_condition_variable_finalize(amp_condition_variable_t cond)
{
    assert(NULL != cond);
    
    if (NULL == cond) {
        return EINVAL;
    }
    
    DeleteCriticalSection(&cond->wake_waiting_threads_critsec);
    DeleteCriticalSection(&cond->access_waiting_threads_count_critsec);
    
    BOOL const close_sem = CloseHandle(cond->waking_waiting_threads_count_control_sem);
    DWORD const close_sem_error = GetLastError();
    BOOL const close_event = CloseHandle(cond->finished_waking_waiting_threads_event);
    DWORD const close_event_error = GetLastError();
    
    int ret_error_code = AMP_SUCCESS;
    
    
    /* If multiple handle closing functions return an error the semaphore error 
     * will hide the event error.
     */
    if (FALSE == close_event) {
        /*
         * TODO: @todo Find out which error codes GetLastError returns when
         *             closing and event handler fails.
         */
        
        ret_error_code = EINVAL;
    }
    
    if (FALSE == close_sem) {
        switch (close_sem_error) {
            case ERROR_SEM_IS_SET:
                /* TODO: @todo Need a Windows dev expert to check if this is the 
                 * right error code interpretation. 
                 */
                assert(false && "Semaphore is in use.");
                
                ret_error_code = EBUSY;
                break;
            case ERROR_SEM_OWNER_DIED:
                /* TODO: @todo Check if this error can really happen. */
                assert(false 
                       && "It shouldn't happen that the previous ownership of the semaphore has ended.");
                ret_error_code = EINVAL;
                break;
            case ERROR_SEM_NOT_FOUND:
                assert(false && "It shouldn't happen that the (not) specified system semaphore name wasn't found.");
                ret_error_code = EINVAL;
                break;
            default:
                /* TODO: @todo Check which code to use to flag an unknown error. 
                 */
                assert(false && "Unknown error.");
                ret_error_code = EINVAL;
                break;
        }
    }
        
    return ret_error_code;
}



int amp_condition_variable_broadcast(amp_condition_variable_t cond)
{
    assert(NULL != cond);
    
    EnterCriticalSection(&cond->wake_waiting_threads_critsec);

    
    
    LONG const waiting_thread_count =  cond->waiting_thread_count;
    
    if (0 < waiting_thread_count) {
        
        cond->broadcast_in_progress = TRUE;

        /* Assuming that less threads exist than max possible semaphore count.
         * TODO: @todo Decide if to spin here if the assumption doesn't hold
         *             true in the future?
         */
        assert(waiting_thread_count <= LONG_MAX 
               && "Assuming that less threads exist than max possible semaphore count.");
        
        /* Releasing the sem here and waiting on it should update the memory of 
         * the waiting threads to see that a broadcast is in progress.
         */
        LONG prev_sem_count = 0;
        BOOL const release_retval = ReleaseSemaphore(cond->waking_waiting_threads_count_control_sem,
                                                     waiting_thread_count,
                                                     &prev_sem_count /* No interest in the previous sem count. */
                                                     );
#if !defined(NDEBUG)
        assert(0l == prev_sem_count);
        assert(TRUE == release_retval);
        if (FALSE == release_retval) {
            cond->broadcast_in_progress = FALSE;
            LeaveCriticalSection(&cond->wake_waiting_threads_critsec);
            
            return EINVAL;
        }
#endif
        
        DWORD const wait_retval = WaitForSingleObject(cond->finished_waking_waiting_threads_event,
                                                     INFINITE);
#if !defined(NDEBUG)
        assert(WAIT_OBJECT_0 == wait_retval);
        if (WAIT_OBJECT_0 != wait_retval) {
            cond->broadcast_in_progress = FALSE;
            LeaveCriticalSection(&cond->wake_waiting_threads_critsec);
            
            return EINVAL;
        }
#endif
        cond->broadcast_in_progress = FALSE;
        
    }
    
    LeaveCriticalSection(&cond->wake_waiting_threads_critsec);
    
    return AMP_SUCCESS;
}



int amp_condition_variable_signal(amp_condition_variable_t cond)
{
    assert(NULL != cond);

    EnterCriticalSection(&cond->wake_waiting_threads_critsec);

    
    BOOL at_least_one_waiting_thread = (0l != cond->waiting_thread_count);
    
    if (at_least_one_waiting_thread) {
        LONG prev_sem_count = 0;
        /* Assuming that less threads exist than max possible semaphore count.
         * TODO: @todo Decide if to spin here if the assumption doesn't hold
         *             true in the future?
         */
        BOOL const release_retval = ReleaseSemaphore(cond->waking_waiting_threads_count_control_sem, 
                                                     1, 
                                                     &prev_sem_count /* No interest in the previous sem count. */
                                                     );
#if !defined(NDEBUG)
        assert(0l == prev_sem_count);
        assert(TRUE == release_retval);
        if (FALSE == release_retval) {
            LeaveCriticalSection(&cond->wake_waiting_threads_critsec);
            
            return EINVAL;
        }
#endif
        
        DWORD const wait_retval = WaitForSingleObject(cond->finished_waking_waiting_threads_event,
                                                     INFINITE);
#if !defined(NDEBUG)
        assert(WAIT_OBJECT_0 == wait_retval);
        if (WAIT_OBJECT_0 != wait_retval) {
            LeaveCriticalSection(&cond->wake_waiting_threads_critsec);
            
            return EINVAL;
        }
#endif
    }
    
    LeaveCriticalSection(&cond->wake_waiting_threads_critsec);
    
    return AMP_SUCCESS;
}



int amp_condition_variable_wait(amp_condition_variable_t cond,
                                amp_mutex_t mutex)
{
    /*
     * TODO: @todo Rewrite this function to prevent duplication of error 
     *             handling code. Use goto or nested if-else-branches?
     */
     
    assert(NULL != cond);
    assert(NULL != mutex);
    
    /* Get the lock that controls that threads can only add themselves to 
     * the waiting count as long as no other thread is doing so, or as long as
     * broadcast or signal aren't waiting on all previously waiting threads
     * to be woken.
     */
    EnterCriticalSection(&cond->wake_waiting_threads_critsec);

    /* TODO: @todo Decide if to use an atomic increment instruction, possibly 
     *             with aquire semantics to prevent instruction reordering 
     *             inside the function.
     *             The critical section needs to stay though, to coordinate with
     *             signal and broadcast which can't allow new waiters to add
     *             themselves until all previously waiting threads are awake.
     */
    ++(cond->waiting_thread_count);

    
    

    /* Unlock the mutex to allow other threads to add themselves to the waiting
     * count or to allow broadcast or signal to be called while they own the
     * mutex.
     * Must be done before waiting on the semaphore that control how many
     * threads are awoken so no deadlock occurs.
     *
     * Current mutex implementation should assert in debug mode and not
     * return any error in non-debug mode.
     */
    int retval = amp_mutex_unlock(mutex);
#if !defined(NDEBUG)
    assert(EINVAL != retval && "Mutex is invalid.");
    assert(EPERM != retval && "Mutex is owned by another thread.");
    assert(AMP_SUCCESS == retval && "Unexpected error.");
    if (AMP_SUCCESS != retval) {
        --(cond->waiting_thread_count);

        LeaveCriticalSection(&cond->wake_waiting_threads_critsec);
        
        /* EINVAL is returned to signal different errors, e.g. not EPERM. */
        return EINVAL;
    }
#endif
    
    
    LeaveCriticalSection(&cond->wake_waiting_threads_critsec);
    
    /* Assuming that less threads exist than max possible semaphore count.
     * TODO: @todo Decide if to spin here if the assumption doesn't hold
     *             true in the future?
     */
    DWORD const wait_retval = WaitForSingleObject(                                                             cond->waking_waiting_threads_count_control_sem, 
                                                             INFINITE
                                                             );
#if !defined(NDEBUG)
    assert(WAIT_OBJECT_0 == wait_retval);
    if (WAIT_OBJECT_0 != wait_retval) {
        EnterCriticalSection(&cond->wake_waiting_threads_critsec);
        EnterCriticalSection(&cond->access_waiting_threads_count_critsec);
        {
            --(cond->waiting_thread_count);
        }
        LeaveCriticalSection(&cond->access_waiting_threads_count_critsec);
        LeaveCriticalSection(&cond->wake_waiting_threads_critsec);
        
        int retval = amp_mutex_lock(mutex);
        /* Error would surface earlier */
        assert(EINVAL != retval && "Mutex is invalid.");
        /* Error would surface earlier */
        assert(EDEADLK != retval && "Mutex is already locked by this thread.");
        assert(AMP_SUCCESS == retval && "Unexpected error.");
        
        return EINVAL;
    }
#endif

    
    /* Control and synchronize access to the waiting thread counter. It is only
     * accessed from awoken waiting threads.
     *
     * TODO: @todo Decide if to replace the critical section by using 
     *             an atomic decrement instruction, possibly with release
     *             semantics to prevent instruction reordering inside the
     *             function. Then atomic loading of the broadcast_in_progress
     *             member field is also needed.
     */

    BOOL const broadcast_in_progress = cond->broadcast_in_progress; /* FALSE; */
    LONG count = 0;
    EnterCriticalSection(&cond->access_waiting_threads_count_critsec);
    {
        count = --(cond->waiting_thread_count);
        
        /* Accessing this field inside the critical section to be sure to
         * get a synchronized value as set by broadcast.
         * Though after returning from the semaphore wait its value should
         * be valid, too.
         */
        /* broadcast_in_progress = cond->broadcast_in_progress; */
    }
    LeaveCriticalSection(&cond->access_waiting_threads_count_critsec);
    
    BOOL all_waiting_threads_awake = TRUE;
    if (TRUE == broadcast_in_progress && count > 0) {
        all_waiting_threads_awake = FALSE;
    }
    
    if (TRUE == all_waiting_threads_awake) {
        /* Tell the signal or broadcast that all threads to wake up are awake
         * so the signal or broadcast can end and allow new waiters to add
         * themselves to the count.
         */
        BOOL const set_event_retval = SetEvent(cond->finished_waking_waiting_threads_event);
#if !defined(NDEBUG)
        assert(TRUE == set_event_retval);
        if (FALSE == set_event_retval) {
            /* This means that the signal or broadcast call will never return...
             * Their thread is lost, and if they were called from inside the
             * critical section with a locked mutex all threads will deadlock
             * while trying to lock the mutex.
             */
            
            /* Lock the mutex before handing back control to the caller.
             *
             * Current mutex implementation should assert in debug mode and not
             * return any error in non-debug mode.
             */
            int retval = amp_mutex_lock(mutex);
            /* Error would surface earlier */
            assert(EINVAL != retval && "Mutex is invalid.");
            /* Error would surface earlier */
            assert(EDEADLK != retval && "Mutex is already locked by this thread."); 
            assert(AMP_SUCCESS == retval && "Unexpected error.");
            
            return EINVAL;
        }
#endif
    }
    
    
    /* Lock the mutex before handing back control to the caller.
     *
     * Current mutex implementation should assert in debug mode and not
     * return any error in non-debug mode.
     */
    retval = amp_mutex_lock(mutex);
    /* Error would surface earlier */
    assert(EINVAL != retval && "Mutex is invalid.");
    /* Error would surface earlier */
    assert(EDEADLK != retval && "Mutex is already locked by this thread."); 
    assert(AMP_SUCCESS == retval && "Unexpected error.");
    
    return AMP_SUCCESS;
}

