/* IBM_PROLOG_BEGIN_TAG                                                   */
/* This is an automatically generated prolog.                             */
/*                                                                        */
/* $Source: src/sbefw/core/sbeConsole.C $                                 */
/*                                                                        */
/* OpenPOWER sbe Project                                                  */
/*                                                                        */
/* Contributors Listed Below - COPYRIGHT 2018-2019                        */
/* [+] International Business Machines Corp.                              */
/* [+] Raptor Engineering, LLC                                            */
/*                                                                        */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License");        */
/* you may not use this file except in compliance with the License.       */
/* You may obtain a copy of the License at                                */
/*                                                                        */
/*     http://www.apache.org/licenses/LICENSE-2.0                         */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/*                                                                        */
/* IBM_PROLOG_END_TAG                                                     */
#include "sbetrace.H"
#include "fapi2.H"

#include "sbeIPLStatusLPC.H"

#include "p9_perv_scom_addresses.H"
#include "p9_perv_scom_addresses_fld.H"
#include "p9_misc_scom_addresses.H"
#include "p9_misc_scom_addresses_fld.H"

#include "sberegaccess.H"
#include "sbeglobals.H"
#include "p9_lpc_utils.H"

using namespace fapi2;

static uint32_t writeLPCReg(uint8_t i_addr,
                  uint8_t i_data)
{
    uint32_t rc = SBE_SEC_OPERATION_SUCCESSFUL;

    do {
        Target<TARGET_TYPE_PROC_CHIP > proc = plat_getChipTarget();

        buffer<uint32_t> data = 0;
        data.insert(i_data, 0, 8);

        ReturnCode fapiRc = lpc_rw(proc,
                                   LPC_IO_SPACE + i_addr,
                                   sizeof(uint8_t),
                                   false,
                                   false,
                                   data);
        if(fapiRc != FAPI2_RC_SUCCESS)
        {
            rc = SBE_SEC_LPC_ACCESS_FAILED;
            break;
        }
    } while(0);

    return rc;
}

void postPutIStep(char major, char minor)
{
    #define SBE_FUNC "postPutIStep"
    uint32_t rc = SBE_SEC_OPERATION_SUCCESSFUL;
    do {
        rc = writeLPCReg(0x81, major);
        if(rc != SBE_SEC_OPERATION_SUCCESSFUL)
        {
            SBE_ERROR(SBE_FUNC " failure to write IPL status 1");
            break;
        }

        rc = writeLPCReg(0x82, minor);
        if(rc != SBE_SEC_OPERATION_SUCCESSFUL)
        {
            SBE_ERROR(SBE_FUNC " failure to write IPL status 2");
            break;
        }

    } while(0);

    #undef SBE_FUNC
}
