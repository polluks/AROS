/*
    Copyright (C) 2017-2020, The AROS Development Team. All rights reserved.
*/

#include <aros/libcall.h>

#include <proto/lowlevel.h>
#include <proto/cia.h>

#include <exec/types.h>
#include <libraries/lowlevel.h>
#include <hardware/cia.h>
#include <resources/cia.h>

#include "lowlevel_intern.h"
#include "cia_intern.h"
#include "cia_timer.h"

AROS_LH3(VOID, StartTimerInt,
      AROS_LHA(APTR , intHandle, A1),
      AROS_LHA(ULONG, timeInterval, D0),
      AROS_LHA(BOOL , continuous, D1),
      struct LowLevelBase *, LowLevelBase, 16, LowLevel)
{
    AROS_LIBFUNC_INIT

    struct CIABase *CiaBase = (struct CIABase *)LowLevelBase->ll_Arch.llad_CIA.llciat_Base;
    UBYTE volatile *ciacr_ptr;
    UBYTE crflags;
    long long ecv;

    if (intHandle && (timeInterval > 0))
    {
        /* Stop the timer if it is currently running */
        StopTimerInt(intHandle);

        /* convert to EClock's */
        ecv = ((long long)timeInterval * LowLevelBase->ll_Arch.llad_EClockMult) >> 15;
        if ((ecv & 0xFFFF) == 0)
            ecv = 1;

        /*
         * Set the requested interval, and Choose appropriate flags
         * for the used CIA timer...
         */
        if (LowLevelBase->ll_Arch.llad_CIA.llciat_iCRBit == CIAICRB_TA)
        {
            CiaBase->hw->ciatalo = (ecv & 0xFF);
            CiaBase->hw->ciatahi = ((ecv >> 8) & 0xFF);

            ciacr_ptr = &CiaBase->hw->ciacra;
            crflags = CIASTART_A;
            if (!continuous)
                crflags |= CIACRAF_RUNMODE;
        }
        else
        {
            CiaBase->hw->ciatblo = (ecv & 0xFF);
            CiaBase->hw->ciatbhi = ((ecv >> 8) & 0xFF);

            ciacr_ptr = &CiaBase->hw->ciacrb;
            crflags = CIASTART_B;
            if (!continuous)
                crflags |=  CIACRBF_RUNMODE;
        }

        /* .. and start up the timer */
        *ciacr_ptr |= crflags;
    }

    return;

    AROS_LIBFUNC_EXIT
}
