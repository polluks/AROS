/*
    Copyright (C) 1995-2020, The AROS Development Team. All rights reserved.

    Desc:
*/

#include <aros/kernel.h>

#include <kernel_base.h>

#define DEBUG 0
#include <aros/debug.h>
#include "kernel_debug.h"

/*****************************************************************************

    NAME */
#include <proto/kernel.h>

        AROS_LH0I(struct TagItem *, KrnGetBootInfo,

/*  SYNOPSIS */

/*  LOCATION */
        struct KernelBase *, KernelBase, 11, Kernel)

/*  FUNCTION

    INPUTS

    RESULT

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO

    INTERNALS

******************************************************************************/
{
    AROS_LIBFUNC_INIT

    D(bug("[Kernel] KrnGetBootInfo()\n"));
    return BootMsg;

    AROS_LIBFUNC_EXIT
}
