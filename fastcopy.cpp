//<MStar Software>
//******************************************************************************
// MStar Software
// Copyright (c) 2010 - 2014 MStar Semiconductor, Inc. All rights reserved.
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
#define LOG_TAG "FastCopy"

#include <utils/Atomic.h>
#include <utils/Log.h>
#include "fastcopy.h"

using namespace android;

static void capture_range_memcpy(void *dest, const void *src, unsigned length,
                                 unsigned long capture_range_original_line_size,
                                 unsigned long capture_range_required_line_size) {
    int line_num = length / capture_range_original_line_size;

    for (int i = 0; i < line_num; i ++) {
        memcpy(dest, src, capture_range_required_line_size);
        dest = (void*)((char *)dest + capture_range_required_line_size);
        src = (void*)((char *)src + capture_range_original_line_size);
    }
}

void* FastCopy::memcpy_work_thread(void *pdata) {
    memcpy_thread_data *p_thread_data = (memcpy_thread_data*)pdata;
    int ret = 0;

    while (1) {
        pthread_mutex_lock(&p_thread_data->memcpy_thread_mutex);
        if (!p_thread_data->pthis->m_memcpy_pthread_exit) {
            ret = pthread_cond_wait(&p_thread_data->memcpy_thread_condition, &p_thread_data->memcpy_thread_mutex);
        } else {
            ALOGD("muli copy thread exist ");
            pthread_mutex_unlock(&p_thread_data->memcpy_thread_mutex);
            break;
        }
        pthread_mutex_unlock(&p_thread_data->memcpy_thread_mutex);
        if (ret != 0) {
            ALOGE("wait time out,memcpy_work_thread,exiting \n");
            break;
        }

        if (!p_thread_data->pthis->m_memcpy_pthread_exit) {
            if (p_thread_data->memcpy_dst && p_thread_data->memcpy_src && p_thread_data->memcpy_length) {
                if (p_thread_data->pthis->m_isCaptureRangecopy) {
                    capture_range_memcpy(p_thread_data->memcpy_dst, p_thread_data->memcpy_src, p_thread_data->memcpy_length,
                                         p_thread_data->pthis->m_capture_range_original_line_size,
                                         p_thread_data->pthis->m_capture_range_required_line_size);
                } else {
                    memcpy(p_thread_data->memcpy_dst, p_thread_data->memcpy_src,p_thread_data->memcpy_length);
                }
            }
            p_thread_data->memcpy_src = p_thread_data->memcpy_dst = NULL;
        }

        pthread_mutex_lock(&p_thread_data->pthis->memcpy_wait_mutex);
        android_atomic_inc(&p_thread_data->pthis->m_memcpy_complete_count);
        if (p_thread_data->pthis->m_memcpy_complete_count >= p_thread_data->pthis->task_count)
            pthread_cond_signal(&p_thread_data->pthis->memcpy_all_done_condition);
        pthread_mutex_unlock(&p_thread_data->pthis->memcpy_wait_mutex);

        if (p_thread_data->pthis->m_memcpy_pthread_exit)
            break;
    }

    ALOGD("pthread_condition_exiting\n");
    return NULL;
}

FastCopy::FastCopy(int paralTaskCount)
:m_paralTaskCount(paralTaskCount)
{
    m_inited= false;
    if (m_paralTaskCount > DIP_MEM_CPY_THRD_CNT)
        m_paralTaskCount = DIP_MEM_CPY_THRD_CNT;

    m_isCaptureRangecopy = false;
    for (int i = 0; i < m_paralTaskCount - 1; i++) {
        m_work_threads[i].inited =  false;
    }
}

FastCopy::~FastCopy() {

    for (int i = 0; i < m_paralTaskCount - 1; i++) {
        pthread_mutex_lock(&m_work_threads[i].memcpy_thread_mutex);
    }

    m_memcpy_pthread_exit = true;

    for (int i = 0; i < m_paralTaskCount - 1; i++) {
        pthread_mutex_unlock(&m_work_threads[i].memcpy_thread_mutex);
    }

    for (int i = 0; i < m_paralTaskCount - 1; i++) {
        if (m_work_threads[i].inited)
            pthread_cond_signal(&m_work_threads[i].memcpy_thread_condition);
    }
    for (int i = 0; i < m_paralTaskCount - 1; i++) {
        if (m_work_threads[i].inited) {
            pthread_join(m_work_threads[i].memcpy_thread_handle,NULL);
            m_work_threads[i].inited = false;
            pthread_cond_destroy(&m_work_threads[i].memcpy_thread_condition);
            pthread_mutex_destroy(&m_work_threads[i].memcpy_thread_mutex);
        }
    }

    pthread_cond_destroy(&memcpy_all_done_condition);
    pthread_mutex_destroy(&memcpy_wait_mutex);
}

void FastCopy::init() {
    m_memcpy_pthread_exit = false;
    task_count = 0;
    m_memcpy_complete_count = 0;
    m_capture_range_original_line_size = 0;
    m_capture_range_required_line_size = 0;

    for (int i = 0; i < m_paralTaskCount - 1; i++) {
        int ret;
        if (m_work_threads[i].inited) {
            continue;
        }
        m_work_threads[i].pthis = this;
        m_work_threads[i].memcpy_dst = m_work_threads[i].memcpy_src = 0;
        pthread_cond_init(&m_work_threads[i].memcpy_thread_condition, NULL);
        pthread_mutex_init(&m_work_threads[i].memcpy_thread_mutex, NULL);

        ret = pthread_create(&m_work_threads[i].memcpy_thread_handle, NULL, memcpy_work_thread, &m_work_threads[i]);
        if (ret == 0)
            m_work_threads[i].inited = true;
    }

    pthread_cond_init(&memcpy_all_done_condition, NULL);
    pthread_mutex_init(&memcpy_wait_mutex, NULL);
    m_inited = true;
}

void FastCopy::capture_range_copy(void *baseDst, const void *baseSrc, unsigned long size,
                                  bool isCaptureRangecopy, unsigned long capture_range_original_line_size,
                                  unsigned long capture_range_required_line_size) {
    unsigned long step;
    unsigned long dst_step;
    if (isCaptureRangecopy) {
        m_isCaptureRangecopy = true;
        m_capture_range_original_line_size = capture_range_original_line_size;
        m_capture_range_required_line_size = capture_range_required_line_size;
        int line_num = size / capture_range_original_line_size;
        int remainder_line_num = (int)line_num % m_paralTaskCount;

        if (remainder_line_num != 0) {
            step = (size + (m_paralTaskCount - remainder_line_num) * capture_range_original_line_size) / m_paralTaskCount;
            dst_step = capture_range_required_line_size * (line_num + m_paralTaskCount - remainder_line_num) / m_paralTaskCount;
        } else {
            step = size / m_paralTaskCount;
            dst_step = capture_range_required_line_size * line_num / m_paralTaskCount;
        }
    } else {
        step = size / m_paralTaskCount;
        dst_step = step;
    }


    if (!m_inited) {
        init();
    }

    pthread_mutex_lock(&memcpy_wait_mutex);
    m_memcpy_complete_count = 0;
    task_count = m_paralTaskCount-1;
    pthread_mutex_unlock(&memcpy_wait_mutex);

    for (int i = 0;i < m_paralTaskCount; i++) {
        unsigned length;

        if (i==m_paralTaskCount-1) {
            length = size-step*i;
            if (m_isCaptureRangecopy) {
                capture_range_memcpy((void*)((int)baseDst+i*dst_step), (void*)((int)baseSrc+i*step),
                                     length, m_capture_range_original_line_size,
                                     m_capture_range_required_line_size);
            } else {
                memcpy((char *)baseDst+i*dst_step, (char *)baseSrc+i*step, length);
            }
        } else {
            length = step;
            if (m_work_threads[i].inited) {
                pthread_mutex_lock(&m_work_threads[i].memcpy_thread_mutex);
                m_work_threads[i].memcpy_dst = (void*)((char *)baseDst+i*dst_step);
                m_work_threads[i].memcpy_src = (void*)((char *)baseSrc+i*step);
                m_work_threads[i].memcpy_length = length;
                pthread_mutex_unlock(&m_work_threads[i].memcpy_thread_mutex);

                pthread_cond_signal(&m_work_threads[i].memcpy_thread_condition);
            } else {
                pthread_mutex_lock(&memcpy_wait_mutex);
                task_count--;
                pthread_mutex_unlock(&memcpy_wait_mutex);
                if (m_isCaptureRangecopy) {
                    capture_range_memcpy((void*)((int)baseDst+i*dst_step), (void*)((int)baseSrc+i*step), length,
                                         m_capture_range_original_line_size,
                                         m_capture_range_required_line_size);
                } else {
                    memcpy((char *)baseDst+i*dst_step, (char *)baseSrc+i*step, length);
                }
            }
        }
    }

    pthread_mutex_lock(&memcpy_wait_mutex);
    if (m_memcpy_complete_count < task_count) {
        pthread_cond_wait(&memcpy_all_done_condition, &memcpy_wait_mutex);
    }
    pthread_mutex_unlock(&memcpy_wait_mutex);
    m_isCaptureRangecopy = false;
}

void FastCopy::copy(void *baseDst, const void *baseSrc, unsigned long size) {
    unsigned long step = size/m_paralTaskCount;
    if (!m_inited) {
        init();
    }

    pthread_mutex_lock(&memcpy_wait_mutex);
    m_memcpy_complete_count = 0;
    task_count = m_paralTaskCount-1;
    pthread_mutex_unlock(&memcpy_wait_mutex);

    for (int i = 0;i < m_paralTaskCount; i++) {
        unsigned length;

        if (i==m_paralTaskCount-1) {
            length = size-step*i;
            memcpy((char *)baseDst+i*step, (char *)baseSrc+i*step, length);
        } else {
            length = step;
            if (m_work_threads[i].inited) {
                pthread_mutex_lock(&m_work_threads[i].memcpy_thread_mutex);
                m_work_threads[i].memcpy_dst = (void*)((char *)baseDst+i*step);
                m_work_threads[i].memcpy_src = (void*)((char *)baseSrc+i*step);
                m_work_threads[i].memcpy_length = length;
                pthread_mutex_unlock(&m_work_threads[i].memcpy_thread_mutex);

                pthread_cond_signal(&m_work_threads[i].memcpy_thread_condition);
            } else {
                pthread_mutex_lock(&memcpy_wait_mutex);
                task_count--;
                pthread_mutex_unlock(&memcpy_wait_mutex);
                memcpy((char *)baseDst+i*step, (char *)baseSrc+i*step, length);
            }
        }
    }

    pthread_mutex_lock(&memcpy_wait_mutex);
    if (m_memcpy_complete_count < task_count) {
        pthread_cond_wait(&memcpy_all_done_condition, &memcpy_wait_mutex);
    }
    pthread_mutex_unlock(&memcpy_wait_mutex);
}
