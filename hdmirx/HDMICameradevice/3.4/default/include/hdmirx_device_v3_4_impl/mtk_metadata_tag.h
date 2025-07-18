/* Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein is
 * confidential and proprietary to MediaTek Inc. and/or its licensors. Without
 * the prior written permission of MediaTek inc. and/or its licensors, any
 * reproduction, modification, use or disclosure of MediaTek Software, and
 * information contained herein, in whole or in part, shall be strictly
 * prohibited.
 *
 * MediaTek Inc. (C) 2021. All rights reserved.
 *
 * BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
 * THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
 * RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER
 * ON AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL
 * WARRANTIES, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR
 * NONINFRINGEMENT. NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH
 * RESPECT TO THE SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY,
 * INCORPORATED IN, OR SUPPLIED WITH THE MEDIATEK SOFTWARE, AND RECEIVER AGREES
 * TO LOOK ONLY TO SUCH THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO.
 * RECEIVER EXPRESSLY ACKNOWLEDGES THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO
 * OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES CONTAINED IN MEDIATEK
 * SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK SOFTWARE
 * RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
 * STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S
 * ENTIRE AND CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE
 * RELEASED HEREUNDER WILL BE, AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE
 * MEDIATEK SOFTWARE AT ISSUE, OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE
 * CHARGE PAID BY RECEIVER TO MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
 *
 * The following software/firmware and/or related documentation ("MediaTek
 * Software") have been modified by MediaTek Inc. All revisions are subject to
 * any receiver's applicable license agreements with MediaTek Inc.
 */

#ifndef INCLUDE_MTKCAM_HALIF_UTILS_METADATA_TAG_1_X_MTK_METADATA_TAG_H_
#define INCLUDE_MTKCAM_HALIF_UTILS_METADATA_TAG_1_X_MTK_METADATA_TAG_H_

typedef enum mtk_camera_metadata_section {
  /**
   * Mediatek Camera vendor tags.
   *
   * The client of camera hal process could recognize these tags and interact
   * with camera hal process within these tags. Interaction between hal process
   * and client might cause some transfer and IPC overhead.
   *
   *  @par Range
   *       - 0x80000000 - 0x9FFFFFFF
   *
   */
  MTK_VENDOR_TAG_SECTION = 0x8000,
  MTK_CAMERA = 0,
  MTK_VENDOR_SECTION_COUNT,
  MTK_VENDOR_TAG_SECTION_END = 0x9FFF,
} mtk_camera_metadata_section_t;

/**
 * Hierarchy positions in enum space. All vendor extension tags must be
 * defined with tag >= VENDOR_SECTION_START
 */

typedef enum mtk_camera_metadata_section_start {

  MTK_CAMERA_START = (MTK_CAMERA + MTK_VENDOR_TAG_SECTION) << 16

} mtk_camera_metadata_section_start_t;

/**
 * Main enum for defining camera metadata tags.  New entries must always go
 * before the section _END tag to preserve existing enumeration values.  In
 * addition, the name and type of the tag needs to be added to
 * ""
 */
typedef enum mtk_camera_metadata_tag {
  MTK_CAMERA_TYPE = MTK_CAMERA_START,
  MTK_CAMERA_HDMI_RX_INFO,
  MTK_CAMERA_MDP_PROFILE,
  MTK_CAMERA_END,
} mtk_camera_metadata_tag_t;

#endif  // INCLUDE_MTKCAM_HALIF_UTILS_METADATA_TAG_1_X_MTK_METADATA_TAG_H_
