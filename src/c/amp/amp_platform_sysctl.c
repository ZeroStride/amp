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
 * Platform hardware detection via sysctlbyname.
 *
 * On Mac OS X the names to query via sysctlbyname can change from update to 
 * update - the use of NSProcessInfo is safer but does not support detection of
 * simultaneous multithreading (SMT - Hyper-Threading in Intel-lingo).
 *
 * amp_platform_destroy and most of the query functions for amp_platform
 * are implemented in amp_internal_platform.c.
 *
 * See http://developer.apple.com/mac/library/releasenotes/Performance/RN-AffinityAPI/
 *
 * See http://developer.apple.com/mac/library/documentation/Darwin/Reference/ManPages/man3/sysctl.3.html#//apple_ref/doc/man/3/sysctl
 *
 * See http://developer.apple.com/mac/library/documentation/Darwin/Reference/ManPages/man3/sysctlbyname.3.html#//apple_ref/doc/man/3/sysctlbyname
 * 
 * See http://developer.apple.com/mac/library/documentation/Darwin/Conceptual/KernelProgramming/boundaries/boundaries.html#//apple_ref/doc/uid/TP30000905-CH217
 * 
 * See http://www.cocoabuilder.com/archive/cocoa/106114-finding-number-of-processors-and-speed.html
 * 
 * See http://lists.apple.com/archives/Darwin-dev/2007/Jun/msg00088.html
 *
 * See http://lists.apple.com/archives/cocoa-dev/2009/Nov/msg00687.html
 */

#include "amp_platform.h"


#include <sys/types.h>
#include <sys/sysctl.h>
#include <assert.h>
#include <errno.h>


#include "amp_stddef.h"
#include "amp_internal_platform.h"



static char const *hw_logicalcpu_online = "hw.logicalcpu";
static char const *hw_logicalcpu_max = "hw.logicalcpu_max";

static char const *hw_physicalcpu_online = "hw.physicalcpu";
static char const *hw_physicalcpu_max = "hw.physicalcpu_max";



static size_t amp_internal_query_sysctlbyname(char const* query_term);
static size_t amp_internal_query_sysctlbyname(char const* query_term)
{
    size_t result = 0;
    size_t result_size = sizeof(size_t);
    
    int const error_code = sysctlbyname(query_term, 
                                        &result, 
                                        &result_size, 
                                        NULL, /* Don't want to change value */
                                        0 /* Don't want to change value */
                                        );
    
    assert(0 == error_code);
    
    if (0 != error_code) {
        result = 0;
    }
    
    return result;
}



int amp_platform_create(struct amp_platform_s** descr,
                        void* allocator_context,
                        amp_alloc_func_t alloc_func,
                        amp_dealloc_func_t dealloc_func)
{
    assert(NULL != descr);
    assert(NULL != alloc_func);
    assert(NULL != dealloc_func);
    
    if (NULL == descr
        || NULL == alloc_func
        || NULL == dealloc_func) {
        
        return EINVAL;
    }
    
    struct amp_platform_s* temp = (struct amp_platform_s*)alloc_func(allocator_context,
                                                                     sizeof(struct amp_platform_s));
    
    if (NULL == temp) {
        return ENOMEM;
    }
    
    temp->core_count = amp_internal_query_sysctlbyname(hw_physicalcpu_max);
    temp->active_core_count = amp_internal_query_sysctlbyname(hw_physicalcpu_online);
    temp->hwthread_count = amp_internal_query_sysctlbyname(hw_logicalcpu_max);
    temp->active_hwthread_count = amp_internal_query_sysctlbyname(hw_logicalcpu_online);
    
    
    *descr = temp;
    
    return AMP_SUCCESS;
}


