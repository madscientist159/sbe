/* IBM_PROLOG_BEGIN_TAG                                                   */
/* This is an automatically generated prolog.                             */
/*                                                                        */
/* $Source: src/import/chips/p9/procedures/hwp/pm/p9_query_core_access_state.C $ */
/*                                                                        */
/* OpenPOWER sbe Project                                                  */
/*                                                                        */
/* Contributors Listed Below - COPYRIGHT 2016,2018                        */
/* [+] International Business Machines Corp.                              */
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
///
/// @file  p9_query_core_access_state.C
/// @brief Check the stop level for a core and set boolean scanable, scomable parameters
///
// *HWP HWP Owner: Brian Vanderpool <vanderp@us.ibm.com>
// *HWP Backup HWP Owner: Greg Still <stillgs@us.ibm.com>
// *HWP FW Owner:  Sangeetha T S <sangeet2@in.ibm.com>
// *HWP Team: PM
// *HWP Level: 2
// *HWP Consumed by: FSP:HS:SBE
///
///
///
/// @verbatim
/// High-level procedure flow:
///     - For the core target, read the Stop State History register in the PPM
///       and use the actual stop level to determine if the core has power and is being
///       clocked.
/// @endverbatim
///
//------------------------------------------------------------------------------


// ----------------------------------------------------------------------
// Includes
// ----------------------------------------------------------------------

#include "p9_query_core_access_state.H"

#define SSHSRC_STOP_GATED 0
#define NET_CTRL0_FENCED  18

// ----------------------------------------------------------------------
// Procedure Function
// ----------------------------------------------------------------------

fapi2::ReturnCode
p9_query_core_access_state(
    const fapi2::Target<fapi2::TARGET_TYPE_CORE>& i_target,
    bool& o_is_scomable,
    bool& o_is_scanable)
{

    fapi2::buffer<uint64_t> l_csshsrc, l_cpfetsense, l_sisr, l_netCtrl0;
    fapi2::buffer<uint64_t> l_data64;
    uint32_t l_coreStopLevel = 0;
    uint8_t  vdd_pfet_disable_core = 0;
    uint8_t  c_exec_hasclocks = 0;
    uint8_t  c_pc_hasclocks = 0;
    uint8_t  l_chpltNumber = 0;


    FAPI_INF("> p9_query_core_access_state...");
    auto l_eq_target = i_target.getParent<fapi2::TARGET_TYPE_EQ>();

    //Check if quad/core is powered off; if so, indicate
    //not scomable or scannable
    FAPI_TRY(fapi2::getScom(l_eq_target, EQ_PPM_PFSNS, l_data64),
             "Error reading data from EQ_PPM_PFSNS");

    if (l_data64.getBit<EQ_PPM_PFSNS_VDD_PFETS_DISABLED_SENSE>())
    {
        o_is_scomable = 0;
        o_is_scanable = 0;
        return fapi2::current_err;
    }

    FAPI_TRY(fapi2::getScom(i_target, C_PPM_PFSNS, l_data64),
             "Error reading data from C_PPM_PFSNS");

    if (l_data64.getBit<C_PPM_PFSNS_VDD_PFETS_DISABLED_SENSE>())
    {
        o_is_scomable = 0;
        o_is_scanable = 0;
        return fapi2::current_err;
    }



    // Get the stop state from the SSHRC in the CPPM
    FAPI_TRY(fapi2::getScom(i_target, C_PPM_SSHSRC, l_csshsrc), "Error reading data from CPPM SSHSRC");


    // A unit is scomable if clocks are running
    // A unit is scannable if the unit is powered up.

    // Extract the core stop state
    if (l_csshsrc.getBit<SSHSRC_STOP_GATED>() == 1)
    {
        l_csshsrc.extractToRight<uint32_t>(l_coreStopLevel, 8, 4);
    }

    if (l_coreStopLevel == 0)
    {
        // Double check the core isn't in stop 1
        auto l_ex_target = i_target.getParent<fapi2::TARGET_TYPE_EX>();

        FAPI_TRY(fapi2::getScom(l_ex_target, EX_CME_LCL_SISR_SCOM, l_sisr), "Error reading data from CME SISR register");

        FAPI_TRY(FAPI_ATTR_GET(fapi2::ATTR_CHIP_UNIT_POS, i_target, l_chpltNumber),
                 "ERROR: Failed to get the position of the Core:0x%08X",
                 i_target);

        uint32_t l_pos = l_chpltNumber % 2;

        if (l_pos == 0 && l_sisr.getBit<EX_CME_LCL_SISR_PM_STATE_ACTIVE_C0>())
        {
            l_sisr.extractToRight<uint32_t>(l_coreStopLevel, EX_CME_LCL_SISR_PM_STATE_C0, EX_CME_LCL_SISR_PM_STATE_C0_LEN);
        }

        if (l_pos == 1 && l_sisr.getBit<EX_CME_LCL_SISR_PM_STATE_ACTIVE_C1>())
        {
            l_sisr.extractToRight<uint32_t>(l_coreStopLevel, EX_CME_LCL_SISR_PM_STATE_C1, EX_CME_LCL_SISR_PM_STATE_C1_LEN);
        }


    }

    FAPI_INF("Core Stop State: C(%d)", l_coreStopLevel);

    // Set both attributes to 1, then clear them based on the stop state
    o_is_scomable = 1;
    o_is_scanable = 1;


    // STOP1 - NAP
    //  VSU, ISU are clocked off
    if (l_coreStopLevel >= 1)
    {
        o_is_scomable = 0;
    }

    // STOP2 - Fast Sleep
    //   VSU, ISU are clocked off
    //   IFU, LSU are clocked off
    //   PC, Core EPS are clocked off
    if (l_coreStopLevel >= 2)
    {
        o_is_scomable = 0;
    }


    // STOP4 - Deep Sleep  (special exception for stop 9 - lab use only)
    //   VSU, ISU are powered off
    //   IFU, LSU are powered off
    //   PC, Core EPS are powered off
    if (l_coreStopLevel >= 4 && l_coreStopLevel != 9)
    {
        o_is_scanable = 0;
    }

    //----------------------------------------------------------------------------------
    // Read clock status and pfet_sense_disabled to confirm stop state history is accurate
    // If we trust the stop state history, this could be removed to save on code size
    //----------------------------------------------------------------------------------


    FAPI_DBG("   Read CPPM PFETSENSE");
    FAPI_TRY(fapi2::getScom(i_target, C_PPM_PFSNS, l_cpfetsense), "Error reading data from CPPM PFSNS");

    // Extract out the disabled bits
    l_cpfetsense.extractToRight<uint8_t>(vdd_pfet_disable_core, 1, 1);

    FAPI_INF("Core PFET_DISABLE(%d)", vdd_pfet_disable_core);


    // Read clocks running registers
    if (vdd_pfet_disable_core == 0)
    {
        // Get the fence bit for this core from C_NET_CTRL0
        FAPI_TRY(fapi2::getScom(i_target, C_NET_CTRL0, l_netCtrl0), "Error reading data from C_NET_CTRL0");

        if (l_netCtrl0.getBit<NET_CTRL0_FENCED>() == 0)
        {
            FAPI_DBG("   Read Core EPS clock status for core");
            FAPI_TRY(fapi2::getScom(i_target, C_CLOCK_STAT_SL,  l_data64), "Error reading data from C_CLOCK_STAT_SL");

            l_data64.extractToRight<uint8_t>(c_exec_hasclocks, 6, 1);
            // Inverted logic in the HW
            c_exec_hasclocks = !c_exec_hasclocks;
            l_data64.extractToRight<uint8_t>(c_pc_hasclocks,   5, 1);
            // Inverted logic in the HW
            c_pc_hasclocks = !c_pc_hasclocks;
        }
        else
        {
            FAPI_INF("Core Fences are up, so skipped reading the C_CLOCK_STAT_SL Register");
        }
    }

    FAPI_INF("Core Clock Status : PC_HASCLOCKS(%d) EXEC_HASCLOCKS(%d)", c_pc_hasclocks, c_exec_hasclocks);

    FAPI_DBG("Comparing Stop State vs Actual HW settings");

    FAPI_DBG("Core Is Scomable STOP_STATE(%d)  CLKSTAT(%d)", o_is_scomable, c_pc_hasclocks && c_exec_hasclocks);
    FAPI_DBG("Core Is Scanable STOP_STATE(%d)  PFET(%d)", o_is_scanable, !vdd_pfet_disable_core);

    //----------------------------------------------------------------------------------
    // Compare Hardware status vs stop state status.   If there is a mismatch, the HW value overrides the stop state
    //----------------------------------------------------------------------------------


    if (o_is_scomable != ( c_pc_hasclocks && c_exec_hasclocks))
    {
        FAPI_INF("Clock status didn't match stop state, overriding is_scomable status");
        o_is_scomable = ( c_pc_hasclocks && c_exec_hasclocks);
    }

    if (o_is_scanable != (vdd_pfet_disable_core == 0))
    {
        FAPI_INF("PFET status didn't match stop state, overriding is_scanable status");
        o_is_scanable = (vdd_pfet_disable_core == 0);
    }

fapi_try_exit:
    FAPI_INF("< p9_query_core_access_state...");
    return fapi2::current_err;
}
