/*
 *  amp_raw_condition_variable_test.cpp
 *  amp
 *
 *  Created by Björn Knafla on 28.09.09.
 *  Copyright 2009 Bjoern Knafla Parallelization + AI + Gamedev Consulting. All rights reserved.
 *
 */

/**
 * @file
 *
 * Tests the shallow wrapper around condition variables.
 *
 * TODO: @todo Add stress tests (eventually in their own file) with very high
 *             thread counts to potentially trigger problems.
 */

#include <amp/amp_raw_condition_variable.h>



#include <cassert>



#include <UnitTest++.h>


#include <amp/amp_stddef.h>
#include <amp/amp_raw_mutex.h>
#include <amp/amp_raw_semaphore.h>
#include <amp/amp_raw_thread.h>



namespace {
    
    std::size_t const avg_thread_count = 4;
    std::size_t const max_thread_count = 128;
    
} // anonymous namespace




SUITE(amp_raw_condition_variable)
{
    
    
    TEST(init_and_finalize)
    {
        struct amp_raw_condition_variable_s cond;
        
        int retval = amp_raw_condition_variable_init(&cond);
        CHECK_EQUAL(AMP_SUCCESS, retval);
        
        retval = amp_raw_condition_variable_finalize(&cond);
        CHECK_EQUAL(AMP_SUCCESS, retval);
    }
    
    
    
    TEST(no_waiting_thread_and_signal)
    {
        struct amp_raw_condition_variable_s cond;
        
        int retval = amp_raw_condition_variable_init(&cond);
        CHECK_EQUAL(AMP_SUCCESS, retval);
        
        retval = amp_raw_condition_variable_signal(&cond);
        CHECK_EQUAL(AMP_SUCCESS, retval);
        
        retval = amp_raw_condition_variable_finalize(&cond);
        CHECK_EQUAL(AMP_SUCCESS, retval);
    }
    
    namespace {
        
        int const state_initialized_flag = 23;
        int const state_waiting_flag = 77;
        int const state_awake_after_waiting_flag = 312;
        
        struct mutex_with_cond_s {
            struct amp_raw_mutex_s mutex;
            struct amp_raw_condition_variable_s cond;
            struct amp_raw_semaphore_s ready_for_signal_sem;
            int state;
        };
        
        
        void cond_waiting_thread_func(void *ctxt)
        {
            struct mutex_with_cond_s *context = static_cast<struct mutex_with_cond_s*>(ctxt);
            
            int retval = amp_raw_mutex_lock(&context->mutex);
            assert(AMP_SUCCESS == retval);
            
            context->state = state_waiting_flag;
            
            retval = amp_raw_semaphore_signal(&context->ready_for_signal_sem);
            assert(AMP_SUCCESS == retval);
            
            retval = amp_raw_condition_variable_wait(&context->cond, 
                                                    &context->mutex);
            
            context->state = state_awake_after_waiting_flag;
            
            
            retval = amp_raw_mutex_unlock(&context->mutex);
            assert(AMP_SUCCESS == retval);
        }
        
    } // anonymous namespace
    
    TEST(single_waiting_thread_and_signal_from_inside_mutex)
    {
        
        struct mutex_with_cond_s mwc;
        
        
        
        int retval = amp_raw_mutex_init(&mwc.mutex);
        CHECK_EQUAL(AMP_SUCCESS, retval);
        
        retval = amp_raw_condition_variable_init(&mwc.cond);
        CHECK_EQUAL(AMP_SUCCESS, retval);
        
        retval = amp_raw_semaphore_init(&mwc.ready_for_signal_sem, 0);
        CHECK_EQUAL(AMP_SUCCESS, retval);
        
        mwc.state = state_initialized_flag;
        
        // This signal should be lost - no waiting thread.
        retval = amp_raw_condition_variable_signal(&mwc.cond);
        CHECK_EQUAL(AMP_SUCCESS, retval);
        
        struct amp_raw_thread_s thread;
        retval = amp_raw_thread_launch(&thread, &mwc, cond_waiting_thread_func);
        
        retval = amp_raw_semaphore_wait(&mwc.ready_for_signal_sem);
        CHECK_EQUAL(AMP_SUCCESS, retval);
        
        CHECK_EQUAL(state_waiting_flag, mwc.state);
        
        retval = amp_raw_mutex_lock(&mwc.mutex);
        CHECK_EQUAL(AMP_SUCCESS, retval);
        
        retval = amp_raw_condition_variable_signal(&mwc.cond);
        CHECK_EQUAL(AMP_SUCCESS, retval);
        
        retval = amp_raw_mutex_unlock(&mwc.mutex);
        CHECK_EQUAL(AMP_SUCCESS, retval);
     
        retval = amp_raw_thread_join(&thread);
        CHECK_EQUAL(AMP_SUCCESS, retval);
        
        CHECK_EQUAL(state_awake_after_waiting_flag, mwc.state);
        
        retval = amp_raw_condition_variable_finalize(&mwc.cond);
        CHECK_EQUAL(AMP_SUCCESS, retval);
        
        retval = amp_raw_mutex_finalize(&mwc.mutex);
        CHECK_EQUAL(AMP_SUCCESS, retval);
    }
    
    
    
    TEST(single_waiting_thread_and_signal_from_outside_mutex)
    {
        struct mutex_with_cond_s mwc;
        
        mwc.state = state_initialized_flag;
        
        int retval = amp_raw_mutex_init(&mwc.mutex);
        CHECK_EQUAL(AMP_SUCCESS, retval);
        
        retval = amp_raw_condition_variable_init(&mwc.cond);
        CHECK_EQUAL(AMP_SUCCESS, retval);
        
        retval = amp_raw_semaphore_init(&mwc.ready_for_signal_sem, 0);
        CHECK_EQUAL(AMP_SUCCESS, retval);

        // This signal should be lost - no waiting thread.
        retval = amp_raw_condition_variable_signal(&mwc.cond);
        CHECK_EQUAL(AMP_SUCCESS, retval);
        
        struct amp_raw_thread_s thread;
        retval = amp_raw_thread_launch(&thread, &mwc, cond_waiting_thread_func);
        
        retval = amp_raw_semaphore_wait(&mwc.ready_for_signal_sem);
        CHECK_EQUAL(AMP_SUCCESS, retval);
        
        CHECK_EQUAL(state_waiting_flag, mwc.state);
        
        // Give the waiting thread a greater chance to start_waiting.
        retval = amp_raw_thread_yield();
        CHECK_EQUAL(AMP_SUCCESS, retval);
        
        
        
        // Signaling the semaphore and even calling wait by the waiting thread
        // aren't atomic. Loop until the waiting thread really caught the 
        // signal.
        bool waiting_thread_is_awake = false;
        while (! waiting_thread_is_awake) {
            
            retval = amp_raw_condition_variable_signal(&mwc.cond);
            CHECK_EQUAL(AMP_SUCCESS, retval);
            
            int rv = amp_raw_mutex_lock(&mwc.mutex);
            CHECK_EQUAL(AMP_SUCCESS, rv);
            
            if (state_awake_after_waiting_flag == mwc.state) {
                waiting_thread_is_awake = true;
            }
            
            rv = amp_raw_mutex_unlock(&mwc.mutex);
            CHECK_EQUAL(AMP_SUCCESS, rv);
            
            // Give the waiting thread a greater chance to get the mutex.
            // rv = amp_raw_thread_yield();
            // CHECK_EQUAL(AMP_SUCCESS, rv);
            
        }
        
        
        retval = amp_raw_thread_join(&thread);
        CHECK_EQUAL(AMP_SUCCESS, retval);
        
        CHECK_EQUAL(state_awake_after_waiting_flag, mwc.state);
        
        retval = amp_raw_condition_variable_finalize(&mwc.cond);
        CHECK_EQUAL(AMP_SUCCESS, retval);
        
        retval = amp_raw_mutex_finalize(&mwc.mutex);
        CHECK_EQUAL(AMP_SUCCESS, retval);
    }
    
    
    
    namespace {
        
        struct one_cond_wait_threads_common_context_s {
            struct amp_raw_mutex_s mutex;
            struct amp_raw_condition_variable_s cond;
            struct amp_raw_semaphore_s ready_for_signal_sem;
            struct amp_raw_semaphore_s thread_awake_sem;
            std::size_t thread_count;
            std::size_t threads_waiting_count;
        };
        
        struct one_cond_wait_thread_context_s {
            struct one_cond_wait_threads_common_context_s *common;
            int state;
        };
        
        
        void one_cond_wait_multiple_thread_func(void *ctxt)
        {
            struct one_cond_wait_thread_context_s *context = 
                static_cast<struct one_cond_wait_thread_context_s*>(ctxt);
            
            
            int retval = amp_raw_mutex_lock(&context->common->mutex);
            assert(AMP_SUCCESS == retval);
            
            context->state = state_waiting_flag;
            
            std::size_t const waiting_thread_count = ++(context->common->threads_waiting_count);
            
            // If all threads adding themselves to wait call wait on the
            // condition variable and mutex and inform the test thread via
            // the semaphore that the last one is about to wait.
            // Make sure that this is clear: it is not guaranteed that the last
            // thread calling wait has registered with the condition variable
            // before the testing thread signals or broadcasts it...
            if (waiting_thread_count == context->common->thread_count) {
                retval = amp_raw_semaphore_signal(&context->common->ready_for_signal_sem);
                assert(AMP_SUCCESS == retval);
            }
            
            retval = amp_raw_condition_variable_wait(&context->common->cond, 
                                                     &context->common->mutex);
            assert(AMP_SUCCESS == retval);
            context->state = state_awake_after_waiting_flag;
            
            retval = amp_raw_semaphore_signal(&context->common->thread_awake_sem);
            assert(AMP_SUCCESS == retval);
            
            retval = amp_raw_mutex_unlock(&context->common->mutex);
            assert(AMP_SUCCESS == retval);
        }
        
        
        
        struct one_cond_wait_thread_context_fixture {
            one_cond_wait_thread_context_fixture()
            :   threads_common_context()
            ,   thread_contexts(NULL)
            {
                int retval = amp_raw_mutex_init(&threads_common_context.mutex);
                assert(AMP_SUCCESS == retval);
                
                retval = amp_raw_condition_variable_init(&threads_common_context.cond);
                assert(AMP_SUCCESS == retval);
                
                retval = amp_raw_semaphore_init(&threads_common_context.ready_for_signal_sem, 0);
                assert(AMP_SUCCESS == retval);
                
                retval = amp_raw_semaphore_init(&threads_common_context.thread_awake_sem, 0);
                assert(AMP_SUCCESS == retval);
                
                threads_common_context.thread_count = thread_count;
                threads_common_context.threads_waiting_count = 0;
                
                
                thread_contexts = new struct one_cond_wait_thread_context_s[thread_count];
                assert(NULL != thread_contexts);
                
                for (std::size_t i = 0; i < thread_count; ++i) {
                    thread_contexts[i].common = &threads_common_context;
                    thread_contexts[i].state = state_initialized_flag;
                }
            }
            
            ~one_cond_wait_thread_context_fixture()
            {
                delete[] thread_contexts;
                
                int retval = amp_raw_semaphore_finalize(&threads_common_context.thread_awake_sem);
                assert(AMP_SUCCESS == retval);
                
                retval = amp_raw_semaphore_finalize(&threads_common_context.ready_for_signal_sem);
                assert(AMP_SUCCESS == retval);
                
                retval = amp_raw_condition_variable_finalize(&threads_common_context.cond);
                assert(AMP_SUCCESS == retval);
                
                retval = amp_raw_mutex_finalize(&threads_common_context.mutex);
                assert(AMP_SUCCESS == retval);
            }
            
            static std::size_t const thread_count = avg_thread_count;
            
            struct one_cond_wait_threads_common_context_s threads_common_context;
            struct one_cond_wait_thread_context_s *thread_contexts;
        };
        
        
    } // anonymous namespace
    
    
    
    TEST_FIXTURE(one_cond_wait_thread_context_fixture, multiple_waiting_threads_and_signal) 
    {

        struct amp_raw_thread_s threads[thread_count];
        
        
        // Start threads to wait on condition variable
        for (std::size_t i = 0; i < thread_count; ++i) {
            int const launch_retval = amp_raw_thread_launch(&threads[i], 
                                                            &thread_contexts[i],
                                                            one_cond_wait_multiple_thread_func);
            CHECK_EQUAL(AMP_SUCCESS, launch_retval);
        }
        
        
        
        int retval = amp_raw_semaphore_wait(&threads_common_context.ready_for_signal_sem);
        CHECK_EQUAL(AMP_SUCCESS, retval);
        
        // retval = amp_raw_mutex_lock(&threads_common_context.mutex);
        // CHECK_EQUAL(AMP_SUCCESS, retval);
        {
            // Waiting on the semaphore should have synced the memory view of 
            // the threads.
            for (std::size_t i = 0; i < thread_count; ++i) {
                CHECK_EQUAL(state_waiting_flag, thread_contexts[i].state);
            }
        }
        // retval = amp_raw_mutex_unlock(&threads_common_context.mutex);
        // CHECK_EQUAL(AMP_SUCCESS, retval);
        
        
        // Signal while owning the mutex
        for (std::size_t i = 0; i < thread_count/2; ++i) {
            
            retval = amp_raw_mutex_lock(&threads_common_context.mutex);
            CHECK_EQUAL(AMP_SUCCESS, retval);
            {
                retval = amp_raw_condition_variable_signal(&threads_common_context.cond);
                CHECK_EQUAL(AMP_SUCCESS, retval);
            }
            retval = amp_raw_mutex_unlock(&threads_common_context.mutex);
            CHECK_EQUAL(AMP_SUCCESS, retval);
            
            retval = amp_raw_semaphore_wait(&threads_common_context.thread_awake_sem);
            CHECK_EQUAL(AMP_SUCCESS, retval);
            
            // Check that only as many threads are awake as signal has been 
            // called.
            std::size_t awake_threads_count = 0;
            for (std::size_t j = 0; j < thread_count; ++j)  {
                
                if (state_awake_after_waiting_flag == thread_contexts[j].state) {
                    ++awake_threads_count;
                }
            }
            
            CHECK_EQUAL(i + 1, awake_threads_count);
            
        }
        
        
        
        
        
        // Signal without owning the mutex after singaling while owning the
        // mutex so the awake semaphore signals of the thread function have
        // been consumed and the semaphore count isn't greater than zero
        // while the mutex-holding signal method waits on the semaphore
        // to know that the signal has been received and one thread awoke.
        std::size_t signal_count = thread_count / 2;
        for (std::size_t i = thread_count/2; i < thread_count; ++i) {
            
            // Give the waiting thread a greater chance to start waiting.
            retval = amp_raw_thread_yield();
            CHECK_EQUAL(AMP_SUCCESS, retval);
            
            // Signaling the semaphore and calling wait by the waiting thread
            // aren't atomic. Loop until the waiting thread really caught the 
            // signal.
            bool waiting_thread_is_awake = false;
            while (! waiting_thread_is_awake) {

                retval = amp_raw_condition_variable_signal(&threads_common_context.cond);
                CHECK_EQUAL(AMP_SUCCESS, retval);
                
                ++signal_count;
                
                retval = amp_raw_mutex_lock(&threads_common_context.mutex);
                CHECK_EQUAL(AMP_SUCCESS, retval);
                {
                    
                    std::size_t awake_threads_count = 0;
                    
                    for (std::size_t j = 0; j < thread_count; ++j)  {
                
                        if (state_awake_after_waiting_flag == thread_contexts[j].state) {
                            ++awake_threads_count;
                        }
            
                    }
                    
                    // Check that not more threads have been woken up than
                    // signals have been send.
                    CHECK(awake_threads_count <= signal_count);
                    
                    if (i + 1 <= awake_threads_count) {
                        // As many threads have set their state to be awake as
                        // the signal loop should have awakened (or even more).
                        waiting_thread_is_awake = true;
                    }
                
                }
                retval = amp_raw_mutex_unlock(&threads_common_context.mutex);
                CHECK_EQUAL(AMP_SUCCESS, retval);
                
                // Give the waiting thread a greater chance to get the mutex.
                retval = amp_raw_thread_yield();
                CHECK_EQUAL(AMP_SUCCESS, retval);
                
            }
            
        }
        
        
        
        for (std::size_t i = 0; i < thread_count; ++i) {
            CHECK_EQUAL(state_awake_after_waiting_flag, thread_contexts[i].state);
        }
        
        
        
        for (std::size_t i = 0; i < thread_count; ++i) {
            retval = amp_raw_thread_join(&threads[i]);
            CHECK_EQUAL(AMP_SUCCESS, retval);
        }
        
    }

    
    
    TEST(no_waiting_thread_and_broadcast)
    {
        struct amp_raw_condition_variable_s cond;
        
        int retval = amp_raw_condition_variable_init(&cond);
        CHECK_EQUAL(AMP_SUCCESS, retval);
        
        retval = amp_raw_condition_variable_broadcast(&cond);
        CHECK_EQUAL(AMP_SUCCESS, retval);
        
        retval = amp_raw_condition_variable_finalize(&cond);
        CHECK_EQUAL(AMP_SUCCESS, retval);
    }
    
    
    
    TEST(single_waiting_thread_and_broadcast_from_inside_mutex)
    {
        
        struct mutex_with_cond_s mwc;
        
        int retval = amp_raw_mutex_init(&mwc.mutex);
        CHECK_EQUAL(AMP_SUCCESS, retval);
        
        retval = amp_raw_condition_variable_init(&mwc.cond);
        CHECK_EQUAL(AMP_SUCCESS, retval);
        
        retval = amp_raw_semaphore_init(&mwc.ready_for_signal_sem, 0);
        CHECK_EQUAL(AMP_SUCCESS, retval);
        
        mwc.state = state_initialized_flag;
        
        // This broadcast should be lost - no waiting thread.
        retval = amp_raw_condition_variable_broadcast(&mwc.cond);
        CHECK_EQUAL(AMP_SUCCESS, retval);
        
        struct amp_raw_thread_s thread;
        retval = amp_raw_thread_launch(&thread, &mwc, cond_waiting_thread_func);
        
        retval = amp_raw_semaphore_wait(&mwc.ready_for_signal_sem);
        CHECK_EQUAL(AMP_SUCCESS, retval);
        
        CHECK_EQUAL(state_waiting_flag, mwc.state);
        
        retval = amp_raw_mutex_lock(&mwc.mutex);
        CHECK_EQUAL(AMP_SUCCESS, retval);
        
        retval = amp_raw_condition_variable_broadcast(&mwc.cond);
        CHECK_EQUAL(AMP_SUCCESS, retval);
        
        retval = amp_raw_mutex_unlock(&mwc.mutex);
        CHECK_EQUAL(AMP_SUCCESS, retval);
        
        retval = amp_raw_thread_join(&thread);
        CHECK_EQUAL(AMP_SUCCESS, retval);
        
        CHECK_EQUAL(state_awake_after_waiting_flag, mwc.state);
        
        retval = amp_raw_condition_variable_finalize(&mwc.cond);
        CHECK_EQUAL(AMP_SUCCESS, retval);
        
        retval = amp_raw_mutex_finalize(&mwc.mutex);
        CHECK_EQUAL(AMP_SUCCESS, retval); 
    }
    
    
    
    TEST(single_waiting_thread_and_broadcast_from_outside_mutex)
    {
        struct mutex_with_cond_s mwc;
        
        mwc.state = state_initialized_flag;
        
        int retval = amp_raw_mutex_init(&mwc.mutex);
        CHECK_EQUAL(AMP_SUCCESS, retval);
        
        retval = amp_raw_condition_variable_init(&mwc.cond);
        CHECK_EQUAL(AMP_SUCCESS, retval);
        
        retval = amp_raw_semaphore_init(&mwc.ready_for_signal_sem, 0);
        CHECK_EQUAL(AMP_SUCCESS, retval);

        // This broadcast should be lost - no waiting thread.
        retval = amp_raw_condition_variable_broadcast(&mwc.cond);
        CHECK_EQUAL(AMP_SUCCESS, retval);
        
        struct amp_raw_thread_s thread;
        retval = amp_raw_thread_launch(&thread, &mwc, cond_waiting_thread_func);
        
        retval = amp_raw_semaphore_wait(&mwc.ready_for_signal_sem);
        CHECK_EQUAL(AMP_SUCCESS, retval);
        
        CHECK_EQUAL(state_waiting_flag, mwc.state);
        
        // Give the waiting thread a greater chance to start_waiting.
        retval = amp_raw_thread_yield();
        CHECK_EQUAL(AMP_SUCCESS, retval);
        
        
        
        // Broadcasting the semaphore and even calling wait by the waiting 
        // thread aren't atomic. Loop until the waiting thread really caught the 
        // broadcast.
        bool waiting_thread_is_awake = false;
        while (! waiting_thread_is_awake) {
            
            retval = amp_raw_condition_variable_broadcast(&mwc.cond);
            CHECK_EQUAL(AMP_SUCCESS, retval);
            
            int rv = amp_raw_mutex_lock(&mwc.mutex);
            CHECK_EQUAL(AMP_SUCCESS, rv);
            
            if (state_awake_after_waiting_flag == mwc.state) {
                waiting_thread_is_awake = true;
            }
            
            rv = amp_raw_mutex_unlock(&mwc.mutex);
            CHECK_EQUAL(AMP_SUCCESS, rv);
            
            // Give the waiting thread a greater chance to get the mutex.
            // rv = amp_raw_thread_yield();
            // CHECK_EQUAL(AMP_SUCCESS, rv);
            
        }
        
        
        retval = amp_raw_thread_join(&thread);
        CHECK_EQUAL(AMP_SUCCESS, retval);
        
        CHECK_EQUAL(state_awake_after_waiting_flag, mwc.state);
        
        retval = amp_raw_condition_variable_finalize(&mwc.cond);
        CHECK_EQUAL(AMP_SUCCESS, retval);
        
        retval = amp_raw_mutex_finalize(&mwc.mutex);
        CHECK_EQUAL(AMP_SUCCESS, retval);
    }
    
    
    
    namespace {
        
        struct multi_cond_wait_threads_common_context_s {
            struct amp_raw_mutex_s mutex;
            struct amp_raw_condition_variable_s cond;
            struct amp_raw_semaphore_s all_threads_about_to_wait_for_cond_sem;
            std::size_t thread_count;
            std::size_t threads_waiting_count;
            std::size_t wait_cycles_count;
        };
        
        struct multi_cond_wait_thread_context_s {
            struct multi_cond_wait_threads_common_context_s *common;
            struct amp_raw_semaphore_s let_thread_proceed_to_next_wait_cycle_sem;
            int state;
            std::size_t current_wait_cycle;
        };
        
        
        void multi_cond_wait_multiple_thread_func(void *ctxt)
        {
            struct multi_cond_wait_thread_context_s *context = 
            static_cast<struct multi_cond_wait_thread_context_s*>(ctxt);
            
            
            for (std::size_t i = 0; i < context->common->wait_cycles_count; ++i) {
            
            
                int retval = amp_raw_mutex_lock(&context->common->mutex);
                assert(AMP_SUCCESS == retval);
                
                context->state = state_waiting_flag;
                context->current_wait_cycle = i;
                
                std::size_t const waiting_thread_count = ++(context->common->threads_waiting_count);
                
                // If all threads adding themselves to wait call wait on the
                // condition variable and mutex and inform the test thread via
                // the semaphore that the last one is about to wait.
                // Make sure that this is clear: it is not guaranteed that the last
                // thread calling wait has registered with the condition variable
                // before the testing thread signals or broadcasts it without
                // owning the mutex.
                if (waiting_thread_count == context->common->thread_count) {
                    retval = amp_raw_semaphore_signal(&context->common->all_threads_about_to_wait_for_cond_sem);

                    // Reset counter for next cycle.
                    context->common->threads_waiting_count = 0;
                    
                    assert(AMP_SUCCESS == retval);
                }
                
                retval = amp_raw_condition_variable_wait(&context->common->cond, 
                                                         &context->common->mutex);
                assert(AMP_SUCCESS == retval);
                context->state = state_awake_after_waiting_flag;
                
                retval = amp_raw_mutex_unlock(&context->common->mutex);
                assert(AMP_SUCCESS == retval);
                
                retval = amp_raw_semaphore_wait(&context->let_thread_proceed_to_next_wait_cycle_sem);
                assert(AMP_SUCCESS == retval);
                
            }
        }
        
        
        
        struct multi_cond_wait_thread_context_fixture {
            multi_cond_wait_thread_context_fixture()
            :   threads_common_context()
            ,   thread_contexts(NULL)
            {
                int retval = amp_raw_mutex_init(&threads_common_context.mutex);
                assert(AMP_SUCCESS == retval);
                
                retval = amp_raw_condition_variable_init(&threads_common_context.cond);
                assert(AMP_SUCCESS == retval);
                
                retval = amp_raw_semaphore_init(&threads_common_context.all_threads_about_to_wait_for_cond_sem, 0);
                assert(AMP_SUCCESS == retval);
                
                threads_common_context.thread_count = thread_count;
                threads_common_context.threads_waiting_count = 0;
                threads_common_context.wait_cycles_count = 3;
                
                thread_contexts = new struct multi_cond_wait_thread_context_s[thread_count];
                assert(NULL != thread_contexts);
                
                for (std::size_t i = 0; i < thread_count; ++i) {
                    thread_contexts[i].common = &threads_common_context;
                    
                    
                    retval = amp_raw_semaphore_init(&(thread_contexts[i].let_thread_proceed_to_next_wait_cycle_sem), 0);
                    assert(AMP_SUCCESS == retval);
                    
                    thread_contexts[i].state = state_initialized_flag;
                    thread_contexts[i].current_wait_cycle = 0;
                }
            }
            
            ~multi_cond_wait_thread_context_fixture()
            {
                
                for (std::size_t i = 0; i < thread_count; ++i) {
                    int rv = amp_raw_semaphore_finalize(&(thread_contexts[i].let_thread_proceed_to_next_wait_cycle_sem));
                    assert(AMP_SUCCESS == rv);
                }
                
                delete[] thread_contexts;
                
                int retval = amp_raw_semaphore_finalize(&threads_common_context.all_threads_about_to_wait_for_cond_sem);
                assert(AMP_SUCCESS == retval);
                
                retval = amp_raw_condition_variable_finalize(&threads_common_context.cond);
                assert(AMP_SUCCESS == retval);
                
                retval = amp_raw_mutex_finalize(&threads_common_context.mutex);
                assert(AMP_SUCCESS == retval);
            }
            
            static std::size_t const thread_count = avg_thread_count;
            
            struct multi_cond_wait_threads_common_context_s threads_common_context;
            struct multi_cond_wait_thread_context_s *thread_contexts;
        };
        
        
    } // anonymous namespace
    
    
    
    TEST_FIXTURE(multi_cond_wait_thread_context_fixture, multiple_waiting_threads_and_broadcast)
    {
        // Only two wait cycles are needed, one for broadcasting while owning
        // the mutex and one while not owning the mutex.
        threads_common_context.wait_cycles_count = 2;
        
        // thread_count is provided by the fixture.
        struct amp_raw_thread_s threads[thread_count];
        
        
        // Start threads to wait on condition variable
        for (std::size_t i = 0; i < thread_count; ++i) {
            int const launch_retval = amp_raw_thread_launch(&threads[i], 
                                                            &thread_contexts[i],
                                                            multi_cond_wait_multiple_thread_func);
            CHECK_EQUAL(AMP_SUCCESS, launch_retval);
        }
        
        
        
        int retval = amp_raw_semaphore_wait(&threads_common_context.all_threads_about_to_wait_for_cond_sem);
        CHECK_EQUAL(AMP_SUCCESS, retval);
        
        // retval = amp_raw_mutex_lock(&threads_common_context.mutex);
        // CHECK_EQUAL(AMP_SUCCESS, retval);
        {
            // Waiting on the semaphore should have synced the memory view of 
            // the threads.
            for (std::size_t i = 0; i < thread_count; ++i) {
                CHECK_EQUAL(state_waiting_flag, thread_contexts[i].state);
                
                // This is the first (counted as 0) wait cycle.
                CHECK_EQUAL(0u, thread_contexts[i].current_wait_cycle);
            }
        }
        // retval = amp_raw_mutex_unlock(&threads_common_context.mutex);
        // CHECK_EQUAL(AMP_SUCCESS, retval);
        
        

        // Broadcast from within the mutex.    
        retval = amp_raw_mutex_lock(&threads_common_context.mutex);
        CHECK_EQUAL(AMP_SUCCESS, retval);
        {
            retval = amp_raw_condition_variable_broadcast(&threads_common_context.cond);
            CHECK_EQUAL(AMP_SUCCESS, retval);
        }
        retval = amp_raw_mutex_unlock(&threads_common_context.mutex);
        CHECK_EQUAL(AMP_SUCCESS, retval);
        
        
        
        // Let all threads proceed to the next wait cycle.
        for (std::size_t i = 0; i < thread_count; ++i) {
            retval = amp_raw_semaphore_signal(&(thread_contexts[i].let_thread_proceed_to_next_wait_cycle_sem));
            CHECK_EQUAL(AMP_SUCCESS, retval);
        }
        
        
        
        // Wait that all threads are about to wait in the next cycle.
        retval = amp_raw_semaphore_wait(&threads_common_context.all_threads_about_to_wait_for_cond_sem);
        CHECK_EQUAL(AMP_SUCCESS, retval);
        
        
        // Check that all threads entered the second cycle (implicitly already
        // shown by the successful semaphore wait above).
        // retval = amp_raw_mutex_lock(&threads_common_context.mutex);
        // CHECK_EQUAL(AMP_SUCCESS, retval);
        {
            for (std::size_t i = 0; i < thread_count; ++i) {
                // CHECK_EQUAL(state_waiting_flag, thread_contexts[i].state);
                CHECK_EQUAL(1, thread_contexts[i].current_wait_cycle);
            }
        }
        // retval = amp_raw_mutex_unlock(&threads_common_context.mutex);
        // CHECK_EQUAL(AMP_SUCCESS, retval);
        
        
        
        
        // Broadcast without owning the mutex.            
        // Give the waiting thread a greater chance to start waiting.
        retval = amp_raw_thread_yield();
        CHECK_EQUAL(AMP_SUCCESS, retval);
        
        // Broadcasting the semaphore and calling wait by the waiting thread
        // aren't atomic. Loop until all waiting threads really caught the 
        // broadcast.
        bool all_threads_are_awake = false;
        while (! all_threads_are_awake) {
            
            retval = amp_raw_condition_variable_broadcast(&threads_common_context.cond);
            CHECK_EQUAL(AMP_SUCCESS, retval);
            
            retval = amp_raw_mutex_lock(&threads_common_context.mutex);
            CHECK_EQUAL(AMP_SUCCESS, retval);
            {
                
                std::size_t awake_threads_count = 0;
                
                for (std::size_t j = 0; j < thread_count; ++j)  {
                    
                    if (state_awake_after_waiting_flag == thread_contexts[j].state) {
                        ++awake_threads_count;
                    }
                    
                }
                
                if (thread_count ==  awake_threads_count) {
                    // As many threads have set their state to be awake as
                    // the signal loop should have awakened (or even more).
                    all_threads_are_awake = true;
                }
                
            }
            retval = amp_raw_mutex_unlock(&threads_common_context.mutex);
            CHECK_EQUAL(AMP_SUCCESS, retval);
            
            // Give the waiting thread a greater chance to get the mutex.
            retval = amp_raw_thread_yield();
            CHECK_EQUAL(AMP_SUCCESS, retval);
            
        }
        
        
        // Let all threads proceed to the next wait cycle. Wihout this
        // No thread could terminate.
        for (std::size_t i = 0; i < thread_count; ++i) {
            retval = amp_raw_semaphore_signal(&(thread_contexts[i].let_thread_proceed_to_next_wait_cycle_sem));
            CHECK_EQUAL(AMP_SUCCESS, retval);
        }
        
        
        for (std::size_t i = 0; i < thread_count; ++i) {
            retval = amp_raw_thread_join(&threads[i]);
            CHECK_EQUAL(AMP_SUCCESS, retval);
        }
        
        
        for (std::size_t i = 0; i < thread_count; ++i) {
            CHECK_EQUAL(state_awake_after_waiting_flag, thread_contexts[i].state);
            CHECK_EQUAL(1, thread_contexts[i].current_wait_cycle);
        }
        
        
        

    }
    
    
    
    TEST_FIXTURE(multi_cond_wait_thread_context_fixture, wait_signal_signal_broadcast_wait_broadcast_wait_signal_broadcast)
    {
        // Use three wait cycles.
        threads_common_context.wait_cycles_count = 3;
        
        // thread_count is provided by the fixture.
        struct amp_raw_thread_s threads[thread_count];
        
        
        // Start threads to wait on condition variable
        for (std::size_t i = 0; i < thread_count; ++i) {
            int const launch_retval = amp_raw_thread_launch(&threads[i], 
                                                            &thread_contexts[i],
                                                            multi_cond_wait_multiple_thread_func);
            CHECK_EQUAL(AMP_SUCCESS, launch_retval);
        }
        
        ////////////////////////////////////////////////////////////////////////
        // First wait cycle (wait cycle 0)
        ////////////////////////////////////////////////////////////////////////
        
        int retval = amp_raw_semaphore_wait(&threads_common_context.all_threads_about_to_wait_for_cond_sem);
        CHECK_EQUAL(AMP_SUCCESS, retval);
        
        // retval = amp_raw_mutex_lock(&threads_common_context.mutex);
        // CHECK_EQUAL(AMP_SUCCESS, retval);
        {
            // Waiting on the semaphore should have synced the memory view of 
            // the threads.
            for (std::size_t i = 0; i < thread_count; ++i) {
                CHECK_EQUAL(state_waiting_flag, thread_contexts[i].state);
                
                // This is the first (counted as 0) wait cycle.
                CHECK_EQUAL(0u, thread_contexts[i].current_wait_cycle);
            }
        }
        // retval = amp_raw_mutex_unlock(&threads_common_context.mutex);
        // CHECK_EQUAL(AMP_SUCCESS, retval);
        
        
        
        // Signal cond var twice from inside mutex.
        std::size_t const first_wait_cycle_signal_count = 2;
        retval = amp_raw_mutex_lock(&threads_common_context.mutex);
        CHECK_EQUAL(AMP_SUCCESS, retval);
        {
            retval = amp_raw_condition_variable_signal(&threads_common_context.cond);
            CHECK_EQUAL(AMP_SUCCESS, retval);
            
            retval = amp_raw_condition_variable_signal(&threads_common_context.cond);
            CHECK_EQUAL(AMP_SUCCESS, retval);
        }
        retval = amp_raw_mutex_unlock(&threads_common_context.mutex);
        CHECK_EQUAL(AMP_SUCCESS, retval);
        
        // Wait until both signaled threads are truly awake.
        // This blocks the whole test on an error.
        bool all_signaled_threads_are_awake = false;
        do {
            std::size_t awake_thread_count = 0;
            for (std::size_t i = 0; i < thread_count; ++i) {
                if (state_awake_after_waiting_flag == thread_contexts[i].state) {
                    ++awake_thread_count;
                }
            }
            
            if (first_wait_cycle_signal_count == awake_thread_count) {
                all_signaled_threads_are_awake = true;
            }
            
        } while (! all_signaled_threads_are_awake);
        
        
        // Broadcast from within the mutex. 
        retval = amp_raw_mutex_lock(&threads_common_context.mutex);
        CHECK_EQUAL(AMP_SUCCESS, retval);
        {
            retval = amp_raw_condition_variable_broadcast(&threads_common_context.cond);
            CHECK_EQUAL(AMP_SUCCESS, retval);
        }
        retval = amp_raw_mutex_unlock(&threads_common_context.mutex);
        CHECK_EQUAL(AMP_SUCCESS, retval);
        
        // Let all threads proceed to the next wait cycle.
        for (std::size_t i = 0; i < thread_count; ++i) {
            retval = amp_raw_semaphore_signal(&(thread_contexts[i].let_thread_proceed_to_next_wait_cycle_sem));
            CHECK_EQUAL(AMP_SUCCESS, retval);
        }
        
        
        ////////////////////////////////////////////////////////////////////////
        // Second wait cycle (wait cycle 1)
        ////////////////////////////////////////////////////////////////////////
        
        
        
        // Wait that all threads are about to wait in the next cycle.
        retval = amp_raw_semaphore_wait(&threads_common_context.all_threads_about_to_wait_for_cond_sem);
        CHECK_EQUAL(AMP_SUCCESS, retval);
        
        
        // Check that all threads entered the second cycle (implicitly already
        // shown by the successful semaphore wait above).
        // retval = amp_raw_mutex_lock(&threads_common_context.mutex);
        // CHECK_EQUAL(AMP_SUCCESS, retval);
        {
            for (std::size_t i = 0; i < thread_count; ++i) {
                // CHECK_EQUAL(state_waiting_flag, thread_contexts[i].state);
                CHECK_EQUAL(1u, thread_contexts[i].current_wait_cycle);
            }
        }
        // retval = amp_raw_mutex_unlock(&threads_common_context.mutex);
        // CHECK_EQUAL(AMP_SUCCESS, retval);
        
        
        
        
        // Broadcast without owning the mutex.            
        // Give the waiting thread a greater chance to start waiting.
        retval = amp_raw_thread_yield();
        CHECK_EQUAL(AMP_SUCCESS, retval);
        
        // Broadcasting the semaphore and calling wait by the waiting thread
        // aren't atomic. Loop until all waiting threads really caught the 
        // broadcast.
        bool all_threads_are_awake = false;
        while (! all_threads_are_awake) {
            
            retval = amp_raw_condition_variable_broadcast(&threads_common_context.cond);
            CHECK_EQUAL(AMP_SUCCESS, retval);
            
            retval = amp_raw_mutex_lock(&threads_common_context.mutex);
            CHECK_EQUAL(AMP_SUCCESS, retval);
            {
                
                std::size_t awake_threads_count = 0;
                
                for (std::size_t j = 0; j < thread_count; ++j)  {
                    
                    if (state_awake_after_waiting_flag == thread_contexts[j].state) {
                        ++awake_threads_count;
                    }
                    
                }
                
                if (thread_count ==  awake_threads_count) {
                    // As many threads have set their state to be awake as
                    // the signal loop should have awakened (or even more).
                    all_threads_are_awake = true;
                }
                
            }
            retval = amp_raw_mutex_unlock(&threads_common_context.mutex);
            CHECK_EQUAL(AMP_SUCCESS, retval);
            
            // Give the waiting thread a greater chance to get the mutex.
            retval = amp_raw_thread_yield();
            CHECK_EQUAL(AMP_SUCCESS, retval);
            
        }
        
        // Let all threads proceed to the next wait cycle. 
        for (std::size_t i = 0; i < thread_count; ++i) {
            retval = amp_raw_semaphore_signal(&(thread_contexts[i].let_thread_proceed_to_next_wait_cycle_sem));
            CHECK_EQUAL(AMP_SUCCESS, retval);
        }
        
        
        ////////////////////////////////////////////////////////////////////////
        // Third wait cycle (wait cycle 2)
        ////////////////////////////////////////////////////////////////////////
        
        retval = amp_raw_semaphore_wait(&threads_common_context.all_threads_about_to_wait_for_cond_sem);
        CHECK_EQUAL(AMP_SUCCESS, retval);
        
        // retval = amp_raw_mutex_lock(&threads_common_context.mutex);
        // CHECK_EQUAL(AMP_SUCCESS, retval);
        {
            // Waiting on the semaphore should have synced the memory view of 
            // the threads.
            for (std::size_t i = 0; i < thread_count; ++i) {
                CHECK_EQUAL(state_waiting_flag, thread_contexts[i].state);
                
                // This is the first (counted as 0) wait cycle.
                CHECK_EQUAL(2u, thread_contexts[i].current_wait_cycle);
            }
        }
        // retval = amp_raw_mutex_unlock(&threads_common_context.mutex);
        // CHECK_EQUAL(AMP_SUCCESS, retval);
        
        
        
        // Signal cond var twice from inside mutex.
        std::size_t const third_wait_cycle_signal_count = 1;
        
        retval = amp_raw_mutex_lock(&threads_common_context.mutex);
        CHECK_EQUAL(AMP_SUCCESS, retval);
        {
            retval = amp_raw_condition_variable_signal(&threads_common_context.cond);
            CHECK_EQUAL(AMP_SUCCESS, retval);
        }
        retval = amp_raw_mutex_unlock(&threads_common_context.mutex);
        CHECK_EQUAL(AMP_SUCCESS, retval);
        
        // Wait until both signaled threads are truly awake.
        // This blocks the whole test on an error.
        all_signaled_threads_are_awake = false;
        do {
            std::size_t awake_thread_count = 0;
            for (std::size_t i = 0; i < thread_count; ++i) {
                if (state_awake_after_waiting_flag == thread_contexts[i].state) {
                    ++awake_thread_count;
                }
            }
            
            if (third_wait_cycle_signal_count == awake_thread_count) {
                all_signaled_threads_are_awake = true;
            }
            
        } while (! all_signaled_threads_are_awake);
        
        
        // Broadcast from within the mutex. 
        retval = amp_raw_mutex_lock(&threads_common_context.mutex);
        CHECK_EQUAL(AMP_SUCCESS, retval);
        {
            retval = amp_raw_condition_variable_broadcast(&threads_common_context.cond);
            CHECK_EQUAL(AMP_SUCCESS, retval);
        }
        retval = amp_raw_mutex_unlock(&threads_common_context.mutex);
        CHECK_EQUAL(AMP_SUCCESS, retval);
        

        // Let all threads proceed to the next wait cycle. Wihout this
        // No thread could terminate.
        for (std::size_t i = 0; i < thread_count; ++i) {
            retval = amp_raw_semaphore_signal(&(thread_contexts[i].let_thread_proceed_to_next_wait_cycle_sem));
            CHECK_EQUAL(AMP_SUCCESS, retval);
        }
        
        
        ////////////////////////////////////////////////////////////////////////
        // Finish test.
        ////////////////////////////////////////////////////////////////////////
        
        
        
        for (std::size_t i = 0; i < thread_count; ++i) {
            retval = amp_raw_thread_join(&threads[i]);
            CHECK_EQUAL(AMP_SUCCESS, retval);
        }
        
        
        for (std::size_t i = 0; i < thread_count; ++i) {
            CHECK_EQUAL(state_awake_after_waiting_flag, thread_contexts[i].state);
            CHECK_EQUAL(2u, thread_contexts[i].current_wait_cycle);
        }
    }
    
    
    
} // SUITE(amp_raw_condition_variable)


