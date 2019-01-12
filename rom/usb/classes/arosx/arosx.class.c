/*
    Copyright � 2018-2019, The AROS Development Team. All rights reserved.
    $Id$

    Desc: Gamepad (XInput) USB class driver
    Lang: English
*/


#include <aros/libcall.h>

#include "debug.h"

#include "arosx.class.h"

struct Library * AROSXInit(void);

/* /// "Lib Stuff" */
static int libInit(LIBBASETYPEPTR nh)
{

    mybug(-1, ("libInit nh: 0x%08lx SysBase: 0x%08lx\n", nh, SysBase));

	nh->nh_gamepad1 = FALSE;
	nh->nh_gamepad2 = FALSE;
	nh->nh_gamepad3 = FALSE;
	nh->nh_gamepad4 = FALSE;

	InitSemaphore(&nh->nh_gamepadlock);

#define	AROSXBase	nh->nh_AROSXBase

	AROSXBase = AROSXInit();

    if(!AROSXBase)
    {
        mybug(-1, ("libInit: MakeLibrary(\"arosx.library\") failed!\n"));
        return(FALSE);
    }

 	mybug(-1, ("AROSX: 0x%08lx\n", AROSXBase));
	AROS_LC0(ULONG, Dummy1, LIBBASETYPEPTR, AROSXBase, 5, arosx);

    mybug(10, ("libInit: Ok\n"));

    return(TRUE);
}

static int libOpen(LIBBASETYPEPTR nh)
{
    mybug(10, ("libOpen nh: 0x%08lx\n", nh));
    return(TRUE);
}

static int libExpunge(LIBBASETYPEPTR nh)
{
    mybug(10, ("libExpunge nh: 0x%08lx\n", nh));
    //CloseLibrary((struct Library *) UtilityBase);
    return(TRUE);
}

ADD2INITLIB(libInit, 0)
ADD2OPENLIB(libOpen, 0)
ADD2EXPUNGELIB(libExpunge, 0)
/* \\\ */

/*
 * ***********************************************************************
 * * Library functions                                                   *
 * ***********************************************************************
 */

#define ps ps
/* /// "usbAttemptInterfaceBinding()" */
struct NepClassHid * usbAttemptInterfaceBinding(struct NepHidBase *nh, struct PsdInterface *pif)
{
    struct Library *ps;

    struct NepClassHid *nch;
    
    IPTR ifclass;
    IPTR subclass;
    IPTR proto;

    struct PsdConfig *pc;
    struct PsdDevice *pd;

    UBYTE buf[64];
    struct Task *tmptask;

    mybug(0, ("nepHidAttemptInterfaceBinding(%08lx)\n", pif));

    if((ps = OpenLibrary("poseidon.library", 4)))
    {
        if((nch = psdAllocVec(sizeof(struct NepClassHid))))
        {
            nch->nch_ClsBase = nh;
            nch->nch_Device = NULL;
            nch->nch_Interface = pif;

            psdGetAttrs(PGA_INTERFACE, pif,
                        IFA_Class, &ifclass,
                        IFA_SubClass, &subclass,
                        IFA_Protocol, &proto,
                        IFA_Config, &pc,
                        TAG_DONE);

            psdGetAttrs(PGA_CONFIG, pc,
                        CA_Device, &pd,
                        TAG_END);

            /*
                Check to see if it is believed to be an XInput Gamepad interface and if so store the XInput descriptor pdd and the data
                 - We could extend this class to also house code for other XInput devices,
                   but for now it's only XInput Gamepad
            */
            nch->nch_pdd = psdFindDescriptor(pd, NULL, DDA_DescriptorType, 33, DDA_Interface, pif, TAG_END);
            if(((ifclass != 255) || (subclass != 93) || (proto != 1) || (nch->nch_pdd == NULL)))
            {
                mybug(0, ("nepHidAttemptInterfaceBinding(%08lx) %d %d %d Nope!\n", pif, ifclass, subclass, proto));
                psdFreeVec(nch);
                CloseLibrary(ps);
                return(NULL);
            }

            /*
                Make sure the XInput descriptor takes the form we expect
                    [16]   33   16    1    1  [36] [129]  20    3    0    3   19    2    0    3    0
                    [17]   33   16    1    1  [37] [129]  20    3    3    3    4   19    2    8    3    3
                XInput descriptor length has to match with the "nibble count"
                    - XInput "USAGE" seems to take the form of nibbles on the bitmask
                    - Nibble byte count seems to relate to the size of the descriptor (adjusted)
                TODO: Make the class bailout earlier if the interface isn't what we want and clean this mess
            */

            psdGetAttrs(PGA_DESCRIPTOR, nch->nch_pdd, DDA_DescriptorData, &nch->nch_xinput_desc, TAG_END);

            UBYTE nibble_check;
            nibble_check = ( (( (nch->nch_xinput_desc[5]>>1) + (nch->nch_xinput_desc[5] & 1) ) - 2) );

            mybug(-1, ("nepHidAttemptInterfaceBinding(%08lx) Nibble check %d\n", pif, nibble_check));
            nDebugMem(ps, nch->nch_xinput_desc, nch->nch_xinput_desc[0]);

            if( (nch->nch_xinput_desc[6] != 129) | (nibble_check != nch->nch_xinput_desc[0]) ) 
            {
                mybug(-1, ("nepHidAttemptInterfaceBinding(%08lx) Not a gamepad! (that we know of...)\n", pif));
                psdFreeVec(nch);
                CloseLibrary(ps);
                return(NULL);
            }

            /*
            	We have a valid XInput gamepad, hopefully... Let's see if there's a free gamepad slot for it
            */

			ObtainSemaphore(&nh->nh_gamepadlock);

			UBYTE gamepad = 0;

			if(nh->nh_gamepad1 == FALSE) {
				nh->nh_gamepad1 = TRUE;
				gamepad = 1;
			}else if(nh->nh_gamepad2 == FALSE) {
				nh->nh_gamepad2 = TRUE;
				gamepad = 2;
			}else if(nh->nh_gamepad3 == FALSE) {
				nh->nh_gamepad3 = TRUE;
				gamepad = 3;
			}else if(nh->nh_gamepad4 == FALSE) {
				nh->nh_gamepad4 = TRUE;
				gamepad = 4;
			}

			ReleaseSemaphore(&nh->nh_gamepadlock);

			mybug(1, ("nepHidAttemptInterfaceBinding gamepad (%01x)\n", gamepad));

            if(!gamepad)
            {
                mybug(1, ("nepHidAttemptInterfaceBinding gamepad count exceeded\n"));
                psdFreeVec(nch);
                CloseLibrary(ps);
                return(NULL);
            }

        	nch->nch_gamepad = gamepad;

            psdSafeRawDoFmt(buf, 64, "arosx.class.gamepad.%01x", nch->nch_gamepad);
            nch->nch_ReadySignal = SIGB_SINGLE;
            nch->nch_ReadySigTask = FindTask(NULL);
            SetSignal(0, SIGF_SINGLE);
            if((tmptask = psdSpawnSubTask(buf, nHidTask, nch)))
            {
                psdBorrowLocksWait(tmptask, 1UL<<nch->nch_ReadySignal);
                if(nch->nch_Task)
                {
                    nch->nch_ReadySigTask = NULL;
                    //FreeSignal(nch->nch_ReadySignal);
                    psdGetAttrs(PGA_DEVICE, pd, DA_ProductName, &nch->nch_devname, TAG_END);

					psdSafeRawDoFmt(nch->nch_gamepadname, 64, "%s (%01x)", nch->nch_devname, nch->nch_gamepad);

                    psdAddErrorMsg(RETURN_OK, (STRPTR) libname, "Play it again, '%s'!", nch->nch_gamepadname);

                    CloseLibrary(ps);
                    return(nch);
                }
            }
            nch->nch_ReadySigTask = NULL;
            //FreeSignal(nch->nch_ReadySignal);
            psdFreeVec(nch);
        }
        CloseLibrary(ps);
    }

    return(NULL);
}
/* \\\ */

/* /// "usbReleaseInterfaceBinding()" */
void usbReleaseInterfaceBinding(struct NepHidBase *nh, struct NepClassHid *nch)
{
    struct Library *ps;
    struct PsdConfig *pc;
    struct PsdDevice *pd;
    STRPTR devname;

    mybug(1, ("nepHidReleaseInterfaceBinding(%08lx)\n", nch));

    /* Kill the nch_GUITask */
    if(nch->nch_GUITask)
    {
        Signal(nch->nch_GUITask, SIGBREAKF_CTRL_C);
    }

    if((ps = OpenLibrary("poseidon.library", 4)))
    {
        Forbid();
        nch->nch_ReadySignal = SIGB_SINGLE;
        nch->nch_ReadySigTask = FindTask(NULL);
        if(nch->nch_Task)
        {
            Signal(nch->nch_Task, SIGBREAKF_CTRL_C);
        }
        Permit();
        while(nch->nch_Task)
        {
            Wait(1L<<nch->nch_ReadySignal);
        }
        //FreeSignal(nch->nch_ReadySignal);
        psdGetAttrs(PGA_INTERFACE, nch->nch_Interface, IFA_Config, &pc, TAG_END);
        psdGetAttrs(PGA_CONFIG, pc, CA_Device, &pd, TAG_END);
        psdGetAttrs(PGA_DEVICE, pd, DA_ProductName, &devname, TAG_END);
        psdAddErrorMsg(RETURN_OK, (STRPTR) libname, "'%s' fell silent!", devname);

		ObtainSemaphore(&nh->nh_gamepadlock);
		if(nch->nch_gamepad == 1) {
			nh->nh_gamepad1 = FALSE;
		}else if(nch->nch_gamepad == 2) {
			nh->nh_gamepad2 = FALSE;
		}else if(nch->nch_gamepad == 3) {
			nh->nh_gamepad3 = FALSE;
		}else if(nch->nch_gamepad == 4) {
			nh->nh_gamepad4 = FALSE;
		}
		ReleaseSemaphore(&nh->nh_gamepadlock);

        psdFreeVec(nch);
        CloseLibrary(ps);
    }
}
/* \\\ */

/* /// "usbGetAttrsA()" */
AROS_LH3(LONG, usbGetAttrsA,
         AROS_LHA(ULONG, type, D0),
         AROS_LHA(APTR, usbstruct, A0),
         AROS_LHA(struct TagItem *, tags, A1),
         LIBBASETYPEPTR, nh, 5, nep)
{
    AROS_LIBFUNC_INIT

    struct TagItem *ti;
    LONG count = 0;

    mybug(1, ("nepHidGetAttrsA(%ld, %08lx, %08lx)\n", type, usbstruct, tags));
    switch(type)
    {
        case UGA_CLASS:
             if((ti = FindTagItem(UCCA_Priority, tags)))
             {
                 *((SIPTR *) ti->ti_Data) = -100;
                 count++;
             }
             if((ti = FindTagItem(UCCA_Description, tags)))
             {
                 *((STRPTR *) ti->ti_Data) = "Gamepad (XInput)";
                 count++;
             }
             if((ti = FindTagItem(UCCA_HasClassCfgGUI, tags)))
             {
                 *((IPTR *) ti->ti_Data) = FALSE;
                 count++;
             }
             if((ti = FindTagItem(UCCA_HasBindingCfgGUI, tags)))
             {
                 *((IPTR *) ti->ti_Data) = TRUE;
                 count++;
             }
             if((ti = FindTagItem(UCCA_AfterDOSRestart, tags)))
             {
                 *((IPTR *) ti->ti_Data) = FALSE;
                 count++;
             }
             break;
         case UGA_BINDING:
             if((ti = FindTagItem(UCCA_UsingDefaultCfg, tags)))
             {
                 *((IPTR *) ti->ti_Data) = FALSE;
                 count++;
             }
             break;
    }
    return(count);
    AROS_LIBFUNC_EXIT
}
/* \\\ */

/* /// "usbSetAttrsA()" */
AROS_LH3(LONG, usbSetAttrsA,
         AROS_LHA(ULONG, type, D0),
         AROS_LHA(APTR, usbstruct, A0),
         AROS_LHA(struct TagItem *, tags, A1),
         LIBBASETYPEPTR, nh, 6, nep)
{
    AROS_LIBFUNC_INIT
    return(0);
    AROS_LIBFUNC_EXIT
}
/* \\\ */

/* /// "usbDoMethodA()" */
AROS_LH2(IPTR, usbDoMethodA,
         AROS_LHA(ULONG, methodid, D0),
         AROS_LHA(IPTR *, methoddata, A1),
         LIBBASETYPEPTR, nh, 7, nep)
{
    AROS_LIBFUNC_INIT

    mybug(0, ("Do Method %ld\n", methodid));
    switch(methodid)
    {
        case UCM_AttemptInterfaceBinding:
            return((IPTR) usbAttemptInterfaceBinding(nh, (struct PsdInterface *) methoddata[0]));

        case UCM_ForceInterfaceBinding:
            return((IPTR) usbAttemptInterfaceBinding(nh, (struct PsdInterface *) methoddata[0]));

        case UCM_ReleaseInterfaceBinding:
            usbReleaseInterfaceBinding(nh, (struct NepClassHid *) methoddata[0]);
            return(TRUE);

        case UCM_OpenBindingCfgWindow:
            return(nOpenCfgWindow((struct NepClassHid *) methoddata[0]));

        default:
            break;
    }
    return(0);
    AROS_LIBFUNC_EXIT
}
/* \\\ */

/**************************************************************************/

#undef  ps
#define ps nch->nch_Base

/* /// "nHidTask()" */
AROS_UFH0(void, nHidTask)
{
    AROS_USERFUNC_INIT

    struct NepClassHid *nch;

    struct PsdPipe *pp;

    ULONG sigmask;
    ULONG sigs;

    UBYTE *epinbuf;
	UBYTE *ep0buf;

    LONG ioerr;

    ULONG len;

    if((nch = nAllocHid()))
    {

    	epinbuf = nch->nch_EPInBuf;
		ep0buf  = nch->nch_EP0Buf;

        nch->signallost = TRUE;

        Forbid();
        if(nch->nch_ReadySigTask)
        {
            Signal(nch->nch_ReadySigTask, 1L<<nch->nch_ReadySignal);
        }
        Permit();
        sigmask = (1L<<nch->nch_TaskMsgPort->mp_SigBit)|SIGBREAKF_CTRL_C;

        psdDelayMS(2000);

        psdPipeSetup(nch->nch_EP0Pipe, URTF_IN|URTF_VENDOR|URTF_INTERFACE, 0x01, 0x0100, 0x00);

        do {
        	ioerr = psdDoPipe(nch->nch_EP0Pipe, ep0buf, 20);
    	} while(ioerr);

    	mybug(1, ("EP0: %02lx %02lx %02lx %02lx %02lx %02lx %02lx %02lx %02lx %02lx %02lx %02lx %02lx %02lx %02lx %02lx %02lx %02lx %02lx %02lx\n",
            	    ep0buf[0], ep0buf[1], ep0buf[2], ep0buf[3], ep0buf[4], ep0buf[5], ep0buf[6], ep0buf[7], ep0buf[8], ep0buf[9], ep0buf[10],
        	        ep0buf[11], ep0buf[12], ep0buf[13], ep0buf[14], ep0buf[15], ep0buf[16], ep0buf[17], ep0buf[18], ep0buf[19]));

        nch->wireless = (ep0buf[18]&(1<<0))? TRUE:FALSE;

		/*
			:) First

            Wireless Logitech F710
			EP0: 00 14 ff f7 ff ff c0 ff c0 ff c0 ff c0 ff 00 00 00 00 01 00
			 - What we have here is a bitmask for all(?) the inputs
			 - We're the first, I think...
			 - If this holds true then the analog thumb stick values aren't exactly 16-bit wide :)
            EPIn: 00 14 00 00 00 00 80 00 80 00 80 00 80 00 b4 00 55 00 00 00

            Wired Logitech F310
            EP0: 00 14 ff f7 ff ff c0 ff c0 ff c0 ff c0 ff 00 00 00 00 00 00

            XInput descriptors for various gamepads, we have one for this interface in *nch_xinput_desc as a UBYTE array
             - Check if it's an index to the bitmask

            [Gamepad F710 descriptors, wireless]
              0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15  16
             16  33  16   1   1  36 129  20   3   0   3  19   2   0   3   0

            [Gamepad F510 descriptors]
              0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15  16
             16  33  16   1   1  36 129  20   3   0   3  19   2   0   3   0

            [Gamepad F310 descriptors]
              0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15  16
             16  33  16   1   1  36 129  20   3   0   3  19   2   0   3   0

            [Xbox360 Controller, other]
              0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15  16
             16  33  16   1   1  36 129  20   3   0   3  19   2   0   3   0

            [Xbox360 Controller, v1.60]
              0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15  16
             17  33  16   1   1  37 129  20   3   3   3   4  19   2   8   3   3

		*/

        /*
        	Set led ring to gamepad number. Should flash for a while and then lid on constantly.
        */

    	UBYTE *bufout;
    	bufout = nch->nch_EPOutBuf;

    	bufout[0] = 0x01;
    	bufout[1] = 0x03;
    	bufout[2] = nch->nch_gamepad + 1;
    	bufout[3] = 0x00;
    	bufout[4] = 0x00;
    	bufout[5] = 0x00;
    	bufout[6] = 0x00;
    	bufout[7] = 0x00;
		bufout[8] = 0x00;
    	bufout[9] = 0x00;
    	bufout[10] = 0x00;
		bufout[11] = 0x00;

    	psdDoPipe(nch->nch_EPOutPipe, bufout, 12);

		psdSendPipe(nch->nch_EPInPipe, epinbuf, 20);
        do
        {
            sigs = Wait(sigmask);
            while((pp = (struct PsdPipe *) GetMsg(nch->nch_TaskMsgPort)))
            {
                if(pp == nch->nch_EPInPipe)
                {
                    if(!(ioerr = psdGetPipeError(pp)))
                    {
                        len = psdGetPipeActual(pp);
                        nParseMsg(nch, epinbuf, len);
                    } else {
                        mybug(1, ("Int Pipe failed %ld\n", ioerr));
                        psdDelayMS(200);
                    }
                    /*
                    	TODO: One Chinese gamepad doesn't wait for new input but sends data back at once
                    		   - Check if response is the same and not much time has elapsed between and set some babble flag and force wait between calls
                    */
                    //psdDelayMS(1);
                    psdSendPipe(nch->nch_EPInPipe, epinbuf, 20);
                    break;
                }
            }
        } while(!(sigs & SIGBREAKF_CTRL_C));
        mybug(-1, ("Going down the river!\n"));
        psdAbortPipe(nch->nch_EPInPipe);
        psdWaitPipe(nch->nch_EPInPipe);
        nFreeHid(nch);
    }
    
    AROS_USERFUNC_EXIT
}
/* \\\ */

/* /// "nParseMsg()" */
void nParseMsg(struct NepClassHid *nch, UBYTE *buf, ULONG len)
{
/* TODO: Check the input message type... */

/*
    Ta-daa!!
    When Logitech Wireless Gamepad F710 goes to sleep we get this in our endpoint
        Msg: 00 14 00 00 00 00 80 00 80 00 80 00 80 00 84 00 00 00 0 0 00
        bit 4 on byte 14

    And this is the first message after it wakes on "A" button press
        Msg: 00 14 00 00 00 00 80 00 80 00 80 00 80 00 94 00 55 00 00 00
        followed by this, the "A" button msg
        Msg: 00 14 00 10 00 00 80 00 80 00 80 00 80 00 b4 00 55 00 00 00

    Mode LED on
        Msg: 00 14 00 00 00 00 80 00 80 00 80 00 80 00 9c 00 55 00 00 00

    Mode LED off
        Msg: 00 14 00 00 00 00 80 00 80 00 80 00 80 00 94 00 55 00 00 00

    Long vibration (Enables rumble effect on controller)
        Msg: 00 14 00 00 00 00 80 00 80 00 80 00 80 00 b4 00 55 00 00 00

    Short vibration (Disables rumble effect on controller)
        Msg: 00 14 00 00 00 00 80 00 80 00 80 00 80 00 94 00 55 00 00 00

    Taking the controller out of range and we get this (same as removing the battery)
        Msg: 00 14 00 00 00 00 80 00 80 00 80 00 80 00 a4 00 00 00 00 00

    Taking the battery out and the dongle soon sends this
        Msg: 00 14 00 00 00 00 80 00 80 00 80 00 80 00 a4 00 00 00 00 00

    Re-inserting the battery and we get this
        Msg: 00 14 00 00 00 00 80 00 80 00 80 00 80 00 b4 00 55 00 00 00

    Pressing "A" on Logitech (wired) Gamepad F310
        Msg: 00 14 00 10 00 00 80 00 80 00 80 00 80 00 00 00 00 00 00 00

    Toggling mode button on Logitech (wired) Gamepad F310 has no effect on the msg but we get one
        some bit from byte 14 and on could tell if it's a wireless or wired (bit 7 of byte 14)
        Byte 16 might be the battery level on Wireless F710?
        Msg: 00 14 00 00 00 00 80 00 80 00 80 00 80 00 00 00 00 00 00 00
        Msg: 00 14 00 00 00 00 80 00 80 00 80 00 80 00 00 00 00 00 00 00
*/
    mybug(0, ("EPIn: %02lx %02lx %02lx %02lx %02lx %02lx %02lx %02lx %02lx %02lx %02lx %02lx %02lx %02lx %02lx %02lx %02lx %02lx %02lx %02lx\n",
                    buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7], buf[8], buf[9], buf[10],
                    buf[11], buf[12], buf[13], buf[14], buf[15], buf[16], buf[17], buf[18], buf[19]));

    /*
        Works at least with Logitech F710
    */
	nch->signallost = (buf[14]&(1<<4))? FALSE:TRUE;

 	/*
    	This will map everything according to Microsoft game controller API
    */
    nch->nch_arosx_gamepad.Buttons = (buf[2]<<0) | (buf[3]<<8);
    nch->nch_arosx_gamepad.LeftTrigger = (buf[4]);
	nch->nch_arosx_gamepad.RightTrigger = (buf[5]);
	nch->nch_arosx_gamepad.ThumbLX = ((buf[6])  | (buf[7]<<8));
	nch->nch_arosx_gamepad.ThumbLY = ((buf[8])  | (buf[9]<<8));
	nch->nch_arosx_gamepad.ThumbRX = ((buf[10]) | (buf[11]<<8));
	nch->nch_arosx_gamepad.ThumbRY = ((buf[12]) | (buf[13]<<8));

    /* Rumble effect
    UBYTE *bufout;
    bufout = nch->nch_EPOutBuf;

    bufout[0] = 0x00;
    bufout[1] = 0x08;
    bufout[2] = 0x00;
    bufout[3] = buf[6];
    bufout[4] = buf[7];
    bufout[5] = 0x00;
    bufout[6] = 0x00;
    bufout[7] = 0x00;
    psdDoPipe(nch->nch_EPOutPipe, bufout, 8);
    */

    if(nch->nch_GUITask)
    {
        Signal(nch->nch_GUITask, (ULONG) (1<<nch->nch_TrackingSignal));
    }

}
/* \\\ */

/* /// "nAllocHid()" */
struct NepClassHid * nAllocHid(void)
{
    struct Task *thistask;
    struct NepClassHid *nch;

    thistask = FindTask(NULL);
    nch = thistask->tc_UserData;
    do
    {
        if(!(nch->nch_Base = OpenLibrary("poseidon.library", 4)))
        {
            Alert(AG_OpenLib);
            break;
        }
        psdGetAttrs(PGA_INTERFACE, nch->nch_Interface,
                    IFA_Config, &nch->nch_Config,
                    IFA_InterfaceNum, &nch->nch_IfNum,
                    TAG_END);
        psdGetAttrs(PGA_CONFIG, nch->nch_Config,
                    CA_Device, &nch->nch_Device,
                    TAG_END);

        nch->nch_EPIn = psdFindEndpoint(nch->nch_Interface, NULL,
                                       EA_IsIn, TRUE,
                                       EA_TransferType, USEAF_INTERRUPT,
                                       TAG_END);

        nch->nch_EPOut = psdFindEndpoint(nch->nch_Interface, NULL,
                                       EA_IsIn, FALSE,
                                       EA_TransferType, USEAF_INTERRUPT,
                                       TAG_END);

        if((!nch->nch_EPIn)|(!nch->nch_EPOut))
        {
            mybug(1, ("Ooops!?! No Endpoints defined?\n"));
            psdAddErrorMsg(RETURN_FAIL, (STRPTR) libname,
                           "Failed to get endpoints!");
            break;
        }
        if((nch->nch_InpMsgPort = CreateMsgPort()))
        {
            if((nch->nch_InpIOReq = (struct IOStdReq *) CreateIORequest(nch->nch_InpMsgPort, sizeof(struct IOStdReq))))
            {
                if(!OpenDevice("input.device", 0, (struct IORequest *) nch->nch_InpIOReq, 0))
                {
                    nch->nch_InputBase = (struct Library *) nch->nch_InpIOReq->io_Device;
                    if((nch->nch_TaskMsgPort = CreateMsgPort()))
                    {
                        if((nch->nch_EP0Pipe = psdAllocPipe(nch->nch_Device, nch->nch_TaskMsgPort, NULL)))
                        {
                            if((nch->nch_EPInPipe = psdAllocPipe(nch->nch_Device, nch->nch_TaskMsgPort, nch->nch_EPIn)))
                            {
                                psdSetAttrs(PGA_PIPE, nch->nch_EPInPipe,
                                            PPA_NakTimeout, FALSE,
                                            PPA_AllowRuntPackets, TRUE,
                                            TAG_END);

                                if((nch->nch_EP0Buf = psdAllocVec(100)))
                                {
                                	if((nch->nch_EPInBuf = psdAllocVec(100)))
                                	{
                                    	if((nch->nch_EPOutPipe = psdAllocPipe(nch->nch_Device, nch->nch_TaskMsgPort, nch->nch_EPOut)))
                                    	{
                                        	psdSetAttrs(PGA_PIPE, nch->nch_EPOutPipe,
                                            	PPA_NakTimeout, FALSE,
                                            	PPA_AllowRuntPackets, TRUE,
                                           		TAG_END);

                                        	if((nch->nch_EPOutBuf = psdAllocVec(100)))
                                        	{
                                            	nch->nch_Task = thistask;
                                            	return(nch);
                                        	}
                                        	psdFreePipe(nch->nch_EPOutPipe);
                                    	}
                                    	psdFreeVec(nch->nch_EPInBuf);
                                	}
                                	psdFreeVec(nch->nch_EP0Buf);
                            	}
                                psdFreePipe(nch->nch_EPInPipe);
                            }
                            psdFreePipe(nch->nch_EP0Pipe);
                        }
                        DeleteMsgPort(nch->nch_TaskMsgPort);
                    }
                    CloseDevice((struct IORequest *) nch->nch_InpIOReq);
                }
                DeleteIORequest((struct IORequest *) nch->nch_InpIOReq);
            }
            DeleteMsgPort(nch->nch_InpMsgPort);
        }
    } while(FALSE);
    CloseLibrary(nch->nch_Base);
    Forbid();
    nch->nch_Task = NULL;
    if(nch->nch_ReadySigTask)
    {
        Signal(nch->nch_ReadySigTask, 1L<<nch->nch_ReadySignal);
    }
    return(NULL);
}
/* \\\ */

/* /// "nFreeHid()" */
void nFreeHid(struct NepClassHid *nch)
{
    psdFreeVec(nch->nch_EPOutBuf);
    psdFreeVec(nch->nch_EPInBuf);

    psdFreePipe(nch->nch_EPOutPipe);
    psdFreePipe(nch->nch_EPInPipe);
    psdFreePipe(nch->nch_EP0Pipe);

    DeleteMsgPort(nch->nch_TaskMsgPort);
    CloseDevice((struct IORequest *) nch->nch_InpIOReq);
    DeleteIORequest((struct IORequest *) nch->nch_InpIOReq);
    DeleteMsgPort(nch->nch_InpMsgPort);

    CloseLibrary(nch->nch_Base);

    Forbid();
    nch->nch_Task = NULL;
    if(nch->nch_ReadySigTask)
    {
        Signal(nch->nch_ReadySigTask, 1L<<nch->nch_ReadySignal);
    }
}
/* \\\ */

/**************************************************************************/

#undef ps

/* /// "nOpenCfgWindow()" */
LONG nOpenCfgWindow(struct NepClassHid *nch)
{
    struct Library *ps;
    mybug(10, ("Opening GUI...\n"));
    if(!(ps = OpenLibrary("poseidon.library", 4)))
    {
        return(FALSE);
    }
    Forbid();
    if(!nch->nch_GUITask)
    {
        if((nch->nch_GUITask = psdSpawnSubTask(MOD_NAME_STRING " GUI", nGUITask, nch)))
        {
            Permit();
            CloseLibrary(ps);
            return(TRUE);
        }
    }
    Permit();
    CloseLibrary(ps);
    return(FALSE);
}
/* \\\ */

#undef ps
#undef MUIMasterBase
#define ps PsdBase
#define MUIMasterBase MUIBase

/* /// "nGUITask()" */
AROS_UFH0(void, nGUITask)
{
    AROS_USERFUNC_INIT

    struct Task *thistask;
    struct NepHidBase *nh;
    struct NepClassHid *nch;

    struct Library *MUIBase;
    struct Library *PsdBase;

    thistask = FindTask(NULL);
    nch = thistask->tc_UserData;
    nh = nch->nch_ClsBase;

    ++nh->nh_Library.lib_OpenCnt;
    if((MUIMasterBase = OpenLibrary(MUIMASTER_NAME, MUIMASTER_VMIN)))
    {
        if((ps = OpenLibrary("poseidon.library", 4)))
        {

            nch->nch_App = ApplicationObject,
            MUIA_Application_Title      , (IPTR)libname,
            MUIA_Application_Version    , (IPTR)VERSION_STRING,
            MUIA_Application_Copyright  , (IPTR)"�2018 The AROS Development Team",
            MUIA_Application_Author     , (IPTR)"The AROS Development Team",
            MUIA_Application_Description, (IPTR)"Settings for the arosx.class",
            MUIA_Application_Base       , (IPTR)"AROSX",
            MUIA_Application_HelpFile   , (IPTR)"HELP:Poseidon.guide",
            MUIA_Application_Menustrip  , (IPTR)MenustripObject,
                Child, (IPTR)MenuObjectT((IPTR)"Project"),
                    Child, (IPTR)(nch->nch_AboutMI = MenuitemObject,
                        MUIA_Menuitem_Title, (IPTR)"About...",
                        MUIA_Menuitem_Shortcut, (IPTR)"?",
                        End),
                    End,
                Child, (IPTR)MenuObjectT((IPTR)"Settings"),
                    Child, (IPTR)(nch->nch_UseMI = MenuitemObject,
                        MUIA_Menuitem_Title, (IPTR)"Save",
                        MUIA_Menuitem_Shortcut, (IPTR)"S",
                        End),
                    Child, (IPTR)MenuitemObject,
                        MUIA_Menuitem_Title, (IPTR)NM_BARLABEL,
                        End,
                    Child, (IPTR)(nch->nch_MUIPrefsMI = MenuitemObject,
                        MUIA_Menuitem_Title, (IPTR)"MUI Settings",
                        MUIA_Menuitem_Shortcut, (IPTR)"M",
                        End),
                    End,
                End,

            SubWindow, (IPTR)(nch->nch_MainWindow = WindowObject,
                MUIA_Window_ID   , MAKE_ID('M','A','I','N'),
                MUIA_Window_Title, (IPTR)nch->nch_gamepadname,
                MUIA_HelpNode, (IPTR)libname,

                WindowContents, (IPTR)VGroup,
                    Child, (IPTR)(nch->nch_GamepadGroupObject = ColGroup(2),
                    	GroupFrameT("Gamepad"),
                    	MUIA_Disabled, TRUE,


                    	Child, (IPTR)HGroup,
            			Child, (IPTR)(nch->nch_GamepadObject_button_a = ImageObject,
                			MUIA_Image_FontMatch, TRUE,
                			MUIA_Selected, FALSE,
                			MUIA_ShowSelState, FALSE,
                			MUIA_Image_Spec, MUII_RadioButton,
                			MUIA_Frame, MUIV_Frame_None,
                   			End),

            			Child, (IPTR)(nch->nch_GamepadObject_button_b = ImageObject,
                			MUIA_Image_FontMatch, TRUE,
                			MUIA_Selected, FALSE,
                			MUIA_ShowSelState, FALSE,
                			MUIA_Image_Spec, MUII_RadioButton,
                			MUIA_Frame, MUIV_Frame_None,
                   			End),

            			Child, (IPTR)(nch->nch_GamepadObject_button_x = ImageObject,
                			MUIA_Image_FontMatch, TRUE,
                			MUIA_Selected, FALSE,
                			MUIA_ShowSelState, FALSE,
                			MUIA_Image_Spec, MUII_RadioButton,
                			MUIA_Frame, MUIV_Frame_None,
                   			End),

            			Child, (IPTR)(nch->nch_GamepadObject_button_y = ImageObject,
                			MUIA_Image_FontMatch, TRUE,
                			MUIA_Selected, FALSE,
                			MUIA_ShowSelState, FALSE,
                			MUIA_Image_Spec, MUII_RadioButton,
                			MUIA_Frame, MUIV_Frame_None,
                   			End),

            			Child, (IPTR)(nch->nch_GamepadObject_button_ls = ImageObject,
                			MUIA_Image_FontMatch, TRUE,
                			MUIA_Selected, FALSE,
                			MUIA_ShowSelState, FALSE,
                			MUIA_Image_Spec, MUII_RadioButton,
                			MUIA_Frame, MUIV_Frame_None,
                   			End),

            			Child, (IPTR)(nch->nch_GamepadObject_button_rs = ImageObject,
                			MUIA_Image_FontMatch, TRUE,
                			MUIA_Selected, FALSE,
                			MUIA_ShowSelState, FALSE,
                			MUIA_Image_Spec, MUII_RadioButton,
                			MUIA_Frame, MUIV_Frame_None,
                   			End),

            			Child, (IPTR)(nch->nch_GamepadObject_left_thumb = ImageObject,
                			MUIA_Image_FontMatch, TRUE,
                			MUIA_Selected, FALSE,
                			MUIA_ShowSelState, FALSE,
                			MUIA_Image_Spec, MUII_RadioButton,
                			MUIA_Frame, MUIV_Frame_None,
                   			End),

            			Child, (IPTR)(nch->nch_GamepadObject_right_thumb = ImageObject,
                			MUIA_Image_FontMatch, TRUE,
                			MUIA_Selected, FALSE,
                			MUIA_ShowSelState, FALSE,
                			MUIA_Image_Spec, MUII_RadioButton,
                			MUIA_Frame, MUIV_Frame_None,
                   			End),

            			Child, (IPTR)(nch->nch_GamepadObject_dpad_left = ImageObject,
                			MUIA_Image_FontMatch, TRUE,
                			MUIA_Selected, FALSE,
                			MUIA_ShowSelState, FALSE,
                			MUIA_Image_Spec, MUII_RadioButton,
                			MUIA_Frame, MUIV_Frame_None,
                   			End),

            			Child, (IPTR)(nch->nch_GamepadObject_dpad_right = ImageObject,
                			MUIA_Image_FontMatch, TRUE,
                			MUIA_Selected, FALSE,
                			MUIA_ShowSelState, FALSE,
                			MUIA_Image_Spec, MUII_RadioButton,
                			MUIA_Frame, MUIV_Frame_None,
                   			End),

            			Child, (IPTR)(nch->nch_GamepadObject_dpad_up = ImageObject,
                			MUIA_Image_FontMatch, TRUE,
                			MUIA_Selected, FALSE,
                			MUIA_ShowSelState, FALSE,
                			MUIA_Image_Spec, MUII_RadioButton,
                			MUIA_Frame, MUIV_Frame_None,
                   			End),

            			Child, (IPTR)(nch->nch_GamepadObject_dpad_down = ImageObject,
                			MUIA_Image_FontMatch, TRUE,
                			MUIA_Selected, FALSE,
                			MUIA_ShowSelState, FALSE,
                			MUIA_Image_Spec, MUII_RadioButton,
                			MUIA_Frame, MUIV_Frame_None,
                   			End),

            			Child, (IPTR)(nch->nch_GamepadObject_button_back = ImageObject,
                			MUIA_Image_FontMatch, TRUE,
                			MUIA_Selected, FALSE,
                			MUIA_ShowSelState, FALSE,
                			MUIA_Image_Spec, MUII_RadioButton,
                			MUIA_Frame, MUIV_Frame_None,
                   			End),

            			Child, (IPTR)(nch->nch_GamepadObject_button_start = ImageObject,
                			MUIA_Image_FontMatch, TRUE,
                			MUIA_Selected, FALSE,
                			MUIA_ShowSelState, FALSE,
                			MUIA_Image_Spec, MUII_RadioButton,
                			MUIA_Frame, MUIV_Frame_None,
                   			End),
                        End,

                        Child, (IPTR)(nch->nch_GamepadObject_left_trigger = GaugeObject,
                            GaugeFrame,
                            MUIA_Gauge_Max, 0xff,
                            MUIA_Gauge_InfoText, (IPTR)"%lx",
                            MUIA_Gauge_Horiz, TRUE,
                            MUIA_Gauge_Current, 0,
                            End),

                        Child, (IPTR)(nch->nch_GamepadObject_right_trigger = GaugeObject,
                            GaugeFrame,
                            MUIA_Gauge_Max, 0xff,
                            MUIA_Gauge_InfoText, (IPTR)"%lx",
                            MUIA_Gauge_Horiz, TRUE,
                            MUIA_Gauge_Current, 0,
                            End),

                        Child, (IPTR)(nch->nch_GamepadObject_left_stick_x = GaugeObject,
                            GaugeFrame,
                            MUIA_Gauge_Max, 0xffff,
                            MUIA_Gauge_InfoText, (IPTR)"%lx",
                            MUIA_Gauge_Horiz, TRUE,
                            MUIA_Gauge_Current, 0,
                            End),

                        Child, (IPTR)(nch->nch_GamepadObject_left_stick_y = GaugeObject,
                            GaugeFrame,
                            MUIA_Gauge_Max, 0xffff,
                            MUIA_Gauge_InfoText, (IPTR)"%lx",
                            MUIA_Gauge_Horiz, TRUE,
                            MUIA_Gauge_Current, 0,
                            End),

                        Child, (IPTR)(nch->nch_GamepadObject_right_stick_x = GaugeObject,
                            GaugeFrame,
                            MUIA_Gauge_Max, 0xffff,
                            MUIA_Gauge_InfoText, (IPTR)"%lx",
                            MUIA_Gauge_Horiz, TRUE,
                            MUIA_Gauge_Current, 0,
                            End),

                        Child, (IPTR)(nch->nch_GamepadObject_right_stick_y = GaugeObject,
                            GaugeFrame,
                            MUIA_Gauge_Max, 0xffff,
                            MUIA_Gauge_InfoText, (IPTR)"%lx",
                            MUIA_Gauge_Horiz, TRUE,
                            MUIA_Gauge_Current, 0,
                            End),

                        End),
                    Child, (IPTR)VSpace(0),
                    Child, (IPTR)HGroup,
                        MUIA_Group_SameWidth, TRUE,
                        Child, (IPTR)(nch->nch_UseObj = TextObject, ButtonFrame,
                            MUIA_Background, MUII_ButtonBack,
                            MUIA_CycleChain, 1,
                            MUIA_InputMode, MUIV_InputMode_RelVerify,
                            MUIA_Text_Contents, (IPTR)"\33c Save ",
                            End),
                        Child, (IPTR)(nch->nch_CloseObj = TextObject, ButtonFrame,
                            MUIA_Background, MUII_ButtonBack,
                            MUIA_CycleChain, 1,
                            MUIA_InputMode, MUIV_InputMode_RelVerify,
                            MUIA_Text_Contents, (IPTR)"\33c Use ",
                            End),
                        End,
                    End,
                End),
            End;

            if(nch->nch_App) 
            {
                DoMethod(nch->nch_MainWindow, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
                         nch->nch_App, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
                DoMethod(nch->nch_UseObj, MUIM_Notify, MUIA_Pressed, FALSE,
                         nch->nch_App, 2, MUIM_Application_ReturnID, ID_STORE_CONFIG);
                DoMethod(nch->nch_CloseObj, MUIM_Notify, MUIA_Pressed, FALSE,
                         nch->nch_App, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);

                DoMethod(nch->nch_AboutMI, MUIM_Notify, MUIA_Menuitem_Trigger, MUIV_EveryTime,
                         nch->nch_App, 2, MUIM_Application_ReturnID, ID_ABOUT);
                DoMethod(nch->nch_UseMI, MUIM_Notify, MUIA_Menuitem_Trigger, MUIV_EveryTime,
                         nch->nch_App, 2, MUIM_Application_ReturnID, ID_STORE_CONFIG);
                DoMethod(nch->nch_MUIPrefsMI, MUIM_Notify, MUIA_Menuitem_Trigger, MUIV_EveryTime,
                         nch->nch_App, 2, MUIM_Application_OpenConfigWindow, 0);


                IPTR  isopen = 0;
                IPTR  iconify = 0;
                ULONG sigs;
                ULONG sigmask;
                LONG retid;

                get(nch->nch_App, MUIA_Application_Iconified, &iconify);
                set(nch->nch_MainWindow, MUIA_Window_Open, TRUE);
                get(nch->nch_MainWindow, MUIA_Window_Open, &isopen);

                if((isopen || (!iconify)))
                {
                    nch->nch_TrackingSignal = AllocSignal(-1);
                    sigmask = (1<<nch->nch_TrackingSignal);
                    do
                    {
                        retid = DoMethod(nch->nch_App, MUIM_Application_NewInput, &sigs);
                        switch(retid)
                        {
                            case ID_ABOUT:
                                MUI_RequestA(nch->nch_App, nch->nch_MainWindow, 0, NULL, "Fabulous!", VERSION_STRING, NULL);
                                break;
                        }
                        if(retid == MUIV_Application_ReturnID_Quit)
                        {
                            break;
                        }
                        if(sigs)
                        {
                            sigs = Wait(sigs | sigmask | SIGBREAKF_CTRL_C);
                            if(sigs & SIGBREAKF_CTRL_C)
                            {
                                break;
                            }

							if((ULONG)(1<<nch->nch_TrackingSignal)) {

                                /* TODO: Check if the GUI goes to sleep when the controller says it's sleepy */
								if((nch->wireless)&&(nch->signallost)) {
                                    set(nch->nch_GamepadGroupObject, MUIA_Disabled, (nch->signallost));
                                    //psdDelayMS(10);
                                } else {
                                    set(nch->nch_GamepadGroupObject, MUIA_Disabled, FALSE);

                                    set(nch->nch_GamepadObject_button_a, MUIA_Selected, (nch->nch_arosx_gamepad.Buttons & AROSX_GAMEPAD_A));
                                    set(nch->nch_GamepadObject_button_b, MUIA_Selected, (nch->nch_arosx_gamepad.Buttons & AROSX_GAMEPAD_B));
                                    set(nch->nch_GamepadObject_button_x, MUIA_Selected, (nch->nch_arosx_gamepad.Buttons & AROSX_GAMEPAD_X));
                                    set(nch->nch_GamepadObject_button_y, MUIA_Selected, (nch->nch_arosx_gamepad.Buttons & AROSX_GAMEPAD_Y));
                                    set(nch->nch_GamepadObject_button_ls, MUIA_Selected, (nch->nch_arosx_gamepad.Buttons & AROSX_GAMEPAD_LEFT_SHOULDER));
                                    set(nch->nch_GamepadObject_button_rs, MUIA_Selected, (nch->nch_arosx_gamepad.Buttons & AROSX_GAMEPAD_RIGHT_SHOULDER));
                                    set(nch->nch_GamepadObject_left_thumb, MUIA_Selected, (nch->nch_arosx_gamepad.Buttons & AROSX_GAMEPAD_LEFT_THUMB));
                                    set(nch->nch_GamepadObject_right_thumb, MUIA_Selected, (nch->nch_arosx_gamepad.Buttons & AROSX_GAMEPAD_RIGHT_THUMB));
                                    set(nch->nch_GamepadObject_dpad_left, MUIA_Selected, (nch->nch_arosx_gamepad.Buttons & AROSX_GAMEPAD_DPAD_LEFT));
									set(nch->nch_GamepadObject_dpad_right, MUIA_Selected, (nch->nch_arosx_gamepad.Buttons & AROSX_GAMEPAD_DPAD_RIGHT));
									set(nch->nch_GamepadObject_dpad_up, MUIA_Selected, (nch->nch_arosx_gamepad.Buttons & AROSX_GAMEPAD_DPAD_UP));
									set(nch->nch_GamepadObject_dpad_down, MUIA_Selected, (nch->nch_arosx_gamepad.Buttons & AROSX_GAMEPAD_DPAD_DOWN));
									set(nch->nch_GamepadObject_button_back, MUIA_Selected, (nch->nch_arosx_gamepad.Buttons & AROSX_GAMEPAD_BACK));
									set(nch->nch_GamepadObject_button_start, MUIA_Selected, (nch->nch_arosx_gamepad.Buttons & AROSX_GAMEPAD_START));

                                    set(nch->nch_GamepadObject_left_trigger, MUIA_Gauge_Current, (nch->nch_arosx_gamepad.LeftTrigger));
                                    set(nch->nch_GamepadObject_right_trigger, MUIA_Gauge_Current, (nch->nch_arosx_gamepad.RightTrigger));

                            		if(nch->nch_arosx_gamepad.ThumbLX>=0x8000) {
                                		set(nch->nch_GamepadObject_left_stick_x, MUIA_Gauge_Current, (nch->nch_arosx_gamepad.ThumbLX-0x8000));
                            		} else {
                                		set(nch->nch_GamepadObject_left_stick_x, MUIA_Gauge_Current, (0x8000+nch->nch_arosx_gamepad.ThumbLX));
                            		}

                            		if(nch->nch_arosx_gamepad.ThumbLY>=0x8000) {
                                		set(nch->nch_GamepadObject_left_stick_y, MUIA_Gauge_Current, (nch->nch_arosx_gamepad.ThumbLY-0x8000));
                            		} else {
                                		set(nch->nch_GamepadObject_left_stick_y, MUIA_Gauge_Current, (0x8000+nch->nch_arosx_gamepad.ThumbLY));
                            		}

                            		if(nch->nch_arosx_gamepad.ThumbRX>=0x8000) {
                                		set(nch->nch_GamepadObject_right_stick_x, MUIA_Gauge_Current, (nch->nch_arosx_gamepad.ThumbRX-0x8000));
                            		} else {
                                		set(nch->nch_GamepadObject_right_stick_x, MUIA_Gauge_Current, (0x8000+nch->nch_arosx_gamepad.ThumbRX));
                            		}

                            		if(nch->nch_arosx_gamepad.ThumbRY>=0x8000) {
                                		set(nch->nch_GamepadObject_right_stick_y, MUIA_Gauge_Current, (nch->nch_arosx_gamepad.ThumbRY-0x8000));
                            		} else {
                                		set(nch->nch_GamepadObject_right_stick_y, MUIA_Gauge_Current, (0x8000+nch->nch_arosx_gamepad.ThumbRY));
                            		}

                            		/* 100Hz max. GUI update frequency should be enough for everyone... */
                            		//psdDelayMS(10);
                            	}
                            }
                        }
                    } while(TRUE);
                    set(nch->nch_MainWindow, MUIA_Window_Open, FALSE);
                }

                if(nch->nch_App)
                {
                    MUI_DisposeObject(nch->nch_App);
                    nch->nch_App = NULL;
                }

                if(MUIMasterBase)
                {
                    CloseLibrary(MUIMasterBase);
                    MUIMasterBase = NULL;
                }

                if(ps)
                {
                    CloseLibrary(ps);
                    ps = NULL;
                }
            }
        }
    }

    Forbid();
    FreeSignal(nch->nch_TrackingSignal);
    nch->nch_TrackingSignal = -1;
    nch->nch_GUITask = NULL;
    --nh->nh_Library.lib_OpenCnt;

    AROS_USERFUNC_EXIT
}
/* \\\ */
