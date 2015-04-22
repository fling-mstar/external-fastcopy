//<MStar Software>
//******************************************************************************
// MStar Software
// Copyright (c) 2010 - 2012 MStar Semiconductor, Inc. All rights reserved.
// All software, firmware and related documentation herein ("MStar Software") are
// intellectual property of MStar Semiconductor, Inc. ("MStar") and protected by
// law, including, but not limited to, copyright law and international treaties.
// Any use, modification, reproduction, retransmission, or republication of all
// or part of MStar Software is expressly prohibited, unless prior written
// permission has been granted by MStar.
//
// By accessing, browsing and/or using MStar Software, you acknowledge that you
// have read, understood, and agree, to be bound by below terms ("Terms") and to
// comply with all applicable laws and regulations:
//
// 1. MStar shall retain any and all right, ownership and interest to MStar
//    Software and any modification/derivatives thereof.
//    No right, ownership, or interest to MStar Software and any
//    modification/derivatives thereof is transferred to you under Terms.
//
// 2. You understand that MStar Software might include, incorporate or be
//    supplied together with third party's software and the use of MStar
//    Software may require additional licenses from third parties.
//    Therefore, you hereby agree it is your sole responsibility to separately
//    obtain any and all third party right and license necessary for your use of
//    such third party's software.
//
// 3. MStar Software and any modification/derivatives thereof shall be deemed as
//    MStar's confidential information and you agree to keep MStar's
//    confidential information in strictest confidence and not disclose to any
//    third party.
//
// 4. MStar Software is provided on an "AS IS" basis without warranties of any
//    kind. Any warranties are hereby expressly disclaimed by MStar, including
//    without limitation, any warranties of merchantability, non-infringement of
//    intellectual property rights, fitness for a particular purpose, error free
//    and in conformity with any international standard.  You agree to waive any
//    claim against MStar for any loss, damage, cost or expense that you may
//    incur related to your use of MStar Software.
//    In no event shall MStar be liable for any direct, indirect, incidental or
//    consequential damages, including without limitation, lost of profit or
//    revenues, lost or damage of data, and unauthorized system use.
//    You agree that this Section 4 shall still apply without being affected
//    even if MStar Software has been modified by MStar in accordance with your
//    request or instruction for your use, except otherwise agreed by both
//    parties in writing.
//
// 5. If requested, MStar may from time to time provide technical supports or
//    services in relation with MStar Software to you for your use of
//    MStar Software in conjunction with your or your customer's product
//    ("Services").
//    You understand and agree that, except otherwise agreed by both parties in
//    writing, Services are provided on an "AS IS" basis and the warranty
//    disclaimer set forth in Section 4 above shall apply.
//
// 6. Nothing contained herein shall be construed as by implication, estoppels
//    or otherwise:
//    (a) conferring any license or right to use MStar name, trademark, service
//        mark, symbol or any other identification;
//    (b) obligating MStar or any of its affiliates to furnish any person,
//        including without limitation, you and your customers, any assistance
//        of any kind whatsoever, or any information; or
//    (c) conferring any license or right under any intellectual property right.
//
// 7. These terms shall be governed by and construed in accordance with the laws
//    of Taiwan, R.O.C., excluding its conflict of law rules.
//    Any and all dispute arising out hereof or related hereto shall be finally
//    settled by arbitration referred to the Chinese Arbitration Association,
//    Taipei in accordance with the ROC Arbitration Law and the Arbitration
//    Rules of the Association by three (3) arbitrators appointed in accordance
//    with the said Rules.
//    The place of arbitration shall be in Taipei, Taiwan and the language shall
//    be English.
//    The arbitration award shall be final and binding to both parties.
//
//******************************************************************************
//<MStar Software>

#include <unistd.h>
#include <stdio.h>
#include <fastcopy.h>
#include <utils/Timers.h>
#include <MsOS.h>
#include <MsTypes.h>
#include <mmap.h>

#define DSIZE 8*1024*1024

using namespace android;

void memcpytest(const char * src, bool initDestBuff, unsigned int size, int paralTaskCount, bool needFlush) {
    long long start = 0, end = 0;
    char *dest = NULL;
    if (src == NULL) {
        printf("invalid src\n");
        return;
    }

    dest = new char[size];
    if (dest == NULL) {
        printf("new dest buffer fail\n");
        return;
    }

    FastCopy fastCopy(paralTaskCount);
    fastCopy.init();
    if (needFlush)
        MsOS_Dcache_Flush((MS_U32)src, size);

    if (initDestBuff)
        printf("dest buffer inited ");
    else
        printf("dest buffer not inited ");
    printf("%d threads parallelly used to memcpy \n", paralTaskCount);

    if (initDestBuff)
        memset(dest, 0, size);

    start = systemTime();
    fastCopy.copy(dest, src, size);
    end = systemTime();

    if (0 == memcmp(src, dest, size)) {
        printf("copy success, elapsed time %lld ms\n", (end-start)/1000000LL);
    } else {
        printf("copy fail\n");
    }

    delete dest;
}


void uncacheMemTest() {
    long long start, end;
    int ret;
    char *src = NULL;
    const mmap_info_t *mmapInfo = NULL;

    mmapInfo = mmap_get_info("E_MMAP_ID_DIP_20M");
    if (mmapInfo == NULL) {
        printf("mmap no buffer E_MMAP_ID_DIP_20M\n");
        return;
    }

    if (mmapInfo->size < DSIZE) {
        printf("mmap buffer E_MMAP_ID_DIP_20M too small %lu\n", mmapInfo->size);
        return;
    }

    if (MsOS_PA2KSEG1(mmapInfo->addr) == 0) {
        if (mmapInfo->miuno == 0) {
            ret = MsOS_MPool_Mapping(0, mmapInfo->addr, DSIZE, 1);
        } else if (mmapInfo->miuno == 1) {
            ret = MsOS_MPool_Mapping(1, (mmapInfo->addr - mmap_get_miu0_offset()), DSIZE, 1);
        } else if (mmapInfo->miuno == 2) {
            ret = MsOS_MPool_Mapping(2, (mmapInfo->addr - mmap_get_miu1_offset()), DSIZE, 1);
        }
    }

    if (mmapInfo->size < DSIZE) {
        printf("mmap buffer size too small %lu\n", mmapInfo->size);
        return;
    }

    src = (char *)MsOS_PA2KSEG1((MS_U32)mmapInfo->addr);
    if (src == 0) {
        printf("get buffer va error\n");
        return;
    }
    memset(src, 0xab, DSIZE);

    printf("***********************uncached memory copy test start *************************\n");
    memcpytest(src, true, DSIZE, 1, false);
    memcpytest(src, true, DSIZE, 2, false);
    memcpytest(src, true, DSIZE, 4, false);
    printf("\n\n");
    sleep(1);

    memcpytest(src, false, DSIZE, 1, false);
    memcpytest(src, false, DSIZE, 2, false);
    memcpytest(src, false, DSIZE, 4, false);
    printf("***********************uncached memory copy test end ***************************\n");
}

void cacheMemTest() {
    long long start, end;
    int ret;
    char *src = NULL;
    const mmap_info_t *mmapInfo = NULL;

    mmapInfo = mmap_get_info("E_MMAP_ID_DIP_20M");
    if (mmapInfo == NULL) {
        printf("mmap no buffer E_MMAP_ID_DIP_20M\n");
        return;
    }

    if (mmapInfo->size < DSIZE) {
        printf("mmap buffer E_MMAP_ID_DIP_20M too small %lu\n", mmapInfo->size);
        return;
    }

    if (MsOS_PA2KSEG0(mmapInfo->addr) == 0) {
        if (mmapInfo->miuno == 0) {
            ret = MsOS_MPool_Mapping(0, mmapInfo->addr, DSIZE, 0);
        } else if (mmapInfo->miuno == 1) {
            ret = MsOS_MPool_Mapping(1, (mmapInfo->addr - mmap_get_miu0_offset()), DSIZE, 0);
        } else if (mmapInfo->miuno == 2) {
            ret = MsOS_MPool_Mapping(2, (mmapInfo->addr - mmap_get_miu1_offset()), DSIZE, 0);
        }
    }

    if (mmapInfo->size < DSIZE) {
        printf("mmap buffer size too small %lu\n", mmapInfo->size);
        return;
    }

    src = (char *)MsOS_PA2KSEG0((MS_U32)mmapInfo->addr);
    if (src == 0) {
        printf("get buffer va error\n");
        return;
    }
    memset(src, 0xab, DSIZE);

    printf("***********************cached memory copy test start *************************\n");
    memcpytest(src, true, DSIZE, 1, true);
    memcpytest(src, true, DSIZE, 2, true);
    memcpytest(src, true, DSIZE, 4, true);
    printf("\n\n");
    sleep(1);

    memcpytest(src, false, DSIZE, 1, true);
    memcpytest(src, false, DSIZE, 2, true);
    memcpytest(src, false, DSIZE, 4, true);
    printf("***********************cached memory copy test end ***************************\n");
}


int main(int argc, char** argv)
{
    long long start, end;
    int ret;
    char *src = NULL;
    const mmap_info_t *mmapInfo = NULL;

    MsOS_MPool_Init();
    MsOS_Init();
    mmap_init();

    uncacheMemTest();

    printf("\n\n");
    cacheMemTest();

    return 0;
}
