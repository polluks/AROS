/*
    Copyright (C) 1995-2017, The AROS Development Team. All rights reserved.
*/

#include <proto/exec.h>

#include <stdarg.h>

#include "hostinterface.h"
#include "kernel_base.h"
#include "kernel_intern.h"
#include "kernel_cpu.h"

/*
 * KernelBase is an optional parameter here. During
 * very early startup it can be NULL.
 */
int krnPutC(int chr, struct KernelBase *KernelBase)
{
    int r;

    /*
     * During early boot SysBase may hold some old value (after warm reboot),
     * but KernelBase is always NULL.
     */
    if (KernelBase)
        Forbid();

    r = HostIFace->KPutC(chr);

    if (KernelBase)
        Permit();

    return r;
}
