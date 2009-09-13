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
 * TODO: @todo Manage threads when using cocoa (create at least one NSTask and create an autoreleasepool inside the threads.
 * TODO: @todo Rename this file and its functions to raw threads and create a amp_thread ADT or a amp thread cluster that collects threads and controls the functions running on the thread. Then add semaphores, mutexes, locks, atomic barriers and ops, and perhaps other stuff as needed.
 * TODO: @todo Add docu to say how many threads can run at max and if joined thread slots are recycled or are blocked for the runtime of the app. Decide if the id is really necessary.
 *
 * TODO: @todo Decide, implement and document if amp raw threads surpress 
 *             signals from reaching them.
 */


#ifndef AMP_amp_raw_thread_H
#define AMP_amp_raw_thread_H


#if defined(AMP_USE_PTHREADS)
#   include <pthread.h>
#elif defined(AMP_USE_WINTHREADS)
#   define WIN32_LEAN_AND_MEAN
#   include <windows.h>
#else
#   error Unsupported platform.
#endif



#if defined(__cplusplus)
extern "C" {
#endif

    
    /**
     * Type of the user function to run on the thread.
     * @param context user data passed into the function.
     */
    typedef void (*amp_raw_thread_func_t)(void *context);
    
    /**
     * Treat this type as opaque as its internals will change from version to
     * version!
     */
    struct amp_native_thread_s {
#if defined(AMP_USE_PTHREADS)
        pthread_t thread;
#elif defined(AMP_USE_WINTHREADS)
        HANDLE thread_handle;
        DWORD thread_id;
#else
#   error Unsupported platform.        
#endif
    };
    typedef struct amp_native_thread_s amp_native_thread_t;
    
    /**
     * Treat this type as opaque as its internals will change from version to
     * version!
     */
    struct amp_raw_thread_s {
        amp_raw_thread_func_t thread_func;
        void *thread_func_context;
        
        struct amp_native_thread_s native_thread_description;
        
        int state;
    };
    typedef struct amp_raw_thread_s amp_raw_thread_t;
    
    
    /**
     * Calls platforms thread creation function that might call malloc 
     * internally and launches the thread.
     *
     * thread must be kept alive as long as the thread exectures - until
     * amp_raw_thread_join returned.
     *
     * Mental thread model: a thread is like a spaceship. After launch
     * it needs to explicitly land - otherwise it just crashes with undefined
     * behavior for the whole surrounding of the crash-site.
     *
     * Might call malloc internally to create platform internal thread 
     * representation.
     *
     * If fed with already launched native thread behavior is undefined.
     *
     * TODO: @todo Add restrict to the pointers and docuement it.
     */
    int amp_raw_thread_launch(amp_raw_thread_t *thread, 
                               void *thread_func_context, 
                               amp_raw_thread_func_t thread_func);
    
    /**
     * Waits until the thread stops and frees its OS resources. 
     *
     * If thread hasn't been launched behavior is undefined.
     *
     * thread memory can be freed after successful join.
     */
    int amp_raw_thread_join(amp_raw_thread_t *thread);
    
    
#if defined(__cplusplus)
}    
#endif


#endif /* #ifndef AMP_amp_raw_thread_H */


