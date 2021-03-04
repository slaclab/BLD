#ifndef FCOM_LCLS_XTCAV_H
#define FCOM_LCLS_XTCAV_H

#ifdef __cplusplus
extern "C" {
#endif


/* Macros to access XTCAV fields; use as follows:
 *
 * FcomBlobRef p_XTCAVData;
 *
 *   fcomGetBlob(id, &p_XTCAVData, 0);
 *
 *   float pavg = p_XTCAVData->fcbl_XTCAV_pavg;
 *   float aavg = p_XTCAVData->fcbl_XTCAV_aavg;
 */ 
#if defined(BLD_SXR)

#define fcbl_xtcav_pavg fc_flt[2]
#define fcbl_xtcav_aavg fc_flt[3]

#else

#define fcbl_xtcav_pavg fc_flt[0]
#define fcbl_xtcav_aavg fc_flt[1]

#endif

#define FC_STAT_LLRF_OK_new               0
#define FC_STAT_LLRF_INVAL_PAVG_MASK_new  1  
#define FC_STAT_LLRF_INVAL_AAVG_MASK_new  2

#ifdef __cplusplus
}
#endif

#endif
