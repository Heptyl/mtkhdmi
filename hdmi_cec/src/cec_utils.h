/*
 * MediaTek Inc. (C) 2018. All rights reserved.
 *
 * Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein is
 * confidential and proprietary to MediaTek Inc. and/or its licensors. Without
 * the prior written permission of MediaTek inc. and/or its licensors, any
 * reproduction, modification, use or disclosure of MediaTek Software, and
 * information contained herein, in whole or in part, shall be strictly
 * prohibited.
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

#ifndef HWCEC_CEC_UTILS_H_
#define HWCEC_CEC_UTILS_H_

#ifdef __cplusplus
extern "C" {
#endif


typedef enum
{
    CEC_DEVICE_LA_DEFAULT         =-1,    //CEC device default. Use default LA as source LA, SDK will auto fill the current allocated source LA
    CEC_DEVICE_LA_TV              =0,     //CEC device TV
    CEC_DEVICE_LA_RECORDER1       =1,     //CEC device RECORDER1
    CEC_DEVICE_LA_RECORDER2       =2,     //CEC device RECORDER2
    CEC_DEVICE_LA_TUNER1          =3,     //CEC device TUNER1
    CEC_DEVICE_LA_PLAYBACK1       =4,     //CEC device PLAYBACK1
    CEC_DEVICE_LA_AUDIO_SYS       =5,     //CEC device AUDIO SYSTEM
    CEC_DEVICE_LA_TUNER2          =6,     //CEC device TUNER2
    CEC_DEVICE_LA_TUNER3          =7,     //CEC device TUNER3
    CEC_DEVICE_LA_PLAYBACK2       =8,     //CEC device PLAYBACK2
    CEC_DEVICE_LA_RECORER3        =9,     //CEC device RECORDER3
    CEC_DEVICE_LA_TUNER4          =10,    //CEC device TUNER4
    CEC_DEVICE_LA_PLAYBACK3       =11,    //CEC device PLAYBACK3
    CEC_DEVICE_LA_RESERVED1       =12,    //Reserved use for CEC device
    CEC_DEVICE_LA_RESERVED2       =13,    //Reserved use for CEC device
    CEC_DEVICE_LA_FREE_USE        =14,    //Free use for CEC device
    CEC_DEVICE_LA_UNREGISTERED    =15,    //Unregistered
    CEC_DEVICE_LA_BROADCAST       =15,    //BROADCAST to all CEC devices
    CEC_DEVICE_LA_MAX                     //MAX Logical Addr num
}CEC_DeviceLa_e;

typedef enum
{
    CEC_DEVICE_TYPE_TV              = 0,   //Device Type TV
    CEC_DEVICE_TYPE_RECORDER,              //Device Type Recorder
    CEC_DEVICE_TYPE_RESERVED,              //Reserved
    CEC_DEVICE_TYPE_TUNER,                 //Device Type Tuner
    CEC_DEVICE_TYPE_PLAYBACK,              //Device Type Playback
    CEC_DEVICE_TYPE_AUDIO_SYSTEM,          //Device Type Audio System
    CEC_DEVICE_TYPE_PURE_CEC_SWITCH,       //Device Type CEC Switch
    CEC_DEVICE_TYPE_VIDEO_PROCESSOR,       //Device Type Video Processor
    CEC_DEVICE_TYPE_MAX,
}CEC_DeviceType_e;

typedef enum
{
    CEC_VERSION_1_3A              = 0,  //CEC Version 1.3a
    CEC_VERSION_1_4,                   //CEC Version 1.4
    CEC_VERSION_2_0,                   //CEC Version 2.0
    CEC_VERSION_MAX,
}CEC_Version_e;



typedef enum
{
//----- One Touch Play ----------------------------
    CEC_OP_ACTIVE_SOURCE                         = 0x82,  //Used by a new source to indicate that it has started to transmit a stream or used in response to CEC_OP_RC_REQ_ACTIVE_SOURCE
    CEC_OP_OTP_IMAGE_VIEW_ON                     = 0x04,  //Sent by a source device to the TV whenever it enters the active state
    CEC_OP_OTP_TEXT_VIEW_ON                      = 0x0D,  //As CEC_OP_OTP_IMAGE_VIEW_ON, but should also remove any text, menus and PIP window from the TV's display

//----- Routing Control ---------------------------
    CEC_OP_RC_INACTIVE_SOURCE                    = 0x9D,  //Used by the currently active source to inform the TV that it has no video to be presented to the user, or is going into the standby state as the result of a local user command on the device.
    CEC_OP_RC_REQ_ACTIVE_SOURCE                  = 0x85,  //Used by a new device to discover the status of the system.
    CEC_OP_RC_ROUTING_CHANGE                     = 0x80,  //Sent by a CEC Switch when it is manually switched to inform all other devices on the network that the active route below the switch has changed.
    CEC_OP_RC_ROUTING_INFO                       = 0x81,  //Sent by a CEC Switch to indicate the active route below the switch.
    CEC_OP_RC_SET_STREAM_PATH                    = 0x86,  //Used by the TV to request a streaming path from the specified physical address.
//----- Standby Command ---------------------------
    CEC_OP_STANDBY                               = 0x36,  //Switches one or all devices into the standby state. Can be used as a broadcast message or be addressed to a specific device.
//----- One Touch Record---------------------------
    CEC_OP_OTR_RECORD_ON                         = 0x09,  //Requests a device to stop a recording.
    CEC_OP_OTR_RECORD_OFF                        = 0x0B,  //Attempt to record the specified source.
    CEC_OP_OTR_RECORD_STATUS                     = 0x0A,  //Used by a recording Device to inform the initiator of the message CEC_OP_OTR_RECORD_ON about its status.
    CEC_OP_OTR_RECORD_TV_SCREEN                  = 0x0F,  //Request by the recording device to record the presently displayed source.
//----- Timer programmer -------------------------- CEC1.3a
    CEC_OP_TP_CLEAR_ANALOG_TIMER                 = 0x33,  //Used to clear an Analogue timer block of a device.
    CEC_OP_TP_CLEAR_DIGITAL_TIMER                = 0x99,  //Used to clear a digital timer block of a device.
    CEC_OP_TP_CLEAR_EXT_TIMER                    = 0xA1,  //Used to clear an external timer block of a device.
    CEC_OP_TP_SET_ANALOG_TIMER                   = 0x34,  //Used to set a single timer block on an analogue recording device.
    CEC_OP_TP_SET_DIGITAL_TIMER                  = 0x97,  //Used to set a single timer block on a digital recording device.
    CEC_OP_TP_SET_EXT_TIMER                      = 0xA2,  //Used to set a single timer block to record from an external device.
    CEC_OP_TP_SET_TIMER_PROGRAM_TITLE            = 0x67,  //Used to set the name of a program associated with a timer block. Sent directly after sending a CEC_OP_TP_SET_ANALOG_TIMER or CEC_OP_TP_SET_DIGITAL_TIMER message. The name if then associated with that timer block.
    CEC_OP_TP_TIMER_CLEARED_STATUS               = 0x43,  //Used to give the status of a CEC_OP_TP_CLEAR_ANALOG_TIMER/CEC_OP_TP_CLEAR_DIGITAL_TIMER/CEC_OP_TP_CLEAR_EXT_TIMER message.
    CEC_OP_TP_TIMER_STATUS                       = 0x35,  //Used to send timer status to the initiator of a set timer message.
//----- System Information ------------------------
    CEC_OP_SI_CEC_VERSION                        = 0x9E,  //Used to indicate the version number of the CEC Specification which was used to design the device, in response to a CEC_OP_SI_GET_CEC_VERSION message.
    CEC_OP_SI_GET_CEC_VERSION                    = 0x9F,  //Used by a device to enquire which version number of the CEC Specification was used to design the follower device.
    CEC_OP_SI_GIVE_PHY_ADDR                      = 0x83,  //A request to a device to return its physical address.
    CEC_OP_SI_REPORT_PHY_ADDR                    = 0x84,  //Used to inform all other devices of the mapping between physical and logical address of the initiator.
    CEC_OP_SI_GET_MENU_LANGUAGE                  = 0x91,  //Set by a device capable of character generation(for OSD and Menus) to a TV in the order to discover the currently selected menu language on the TV.
    CEC_OP_SI_SET_MENU_LANGUAGE                  = 0x32,  //Used by a TV to indicate its currently selected menu language.
//----- Deck Control Feature-----------------------
    CEC_OP_DC_DECK_CTRL                          = 0x42,  //Used to control a device's media functions.
    CEC_OP_DC_DECK_STATUS                        = 0x1B,  //Used to provide a deck's status to the initiator of the CEC_OP_DC_GIVE_DECK_STATUS message.
    CEC_OP_DC_GIVE_DECK_STATUS                   = 0x1A,  //Used to request the status of a device, regardless of whether or not it is the current active source.
    CEC_OP_DC_PLAY                               = 0x41,  //Used to control the playback behavior of a source device.
//----- Tuner Control ------------------------------
    CEC_OP_TC_GIVE_TUNER_STATUS                  = 0x08,  //Used to request the status of a tuner device.
    CEC_OP_TC_SEL_ANALOG_SERVICE                 = 0x92,  //Directly selects an analogue TV service.
    CEC_OP_TC_SEL_DIGITAL_SERVICE                = 0x93,  //Directly selects a Ditial TV, Radio or Data Broadcast Service.
    CEC_OP_TC_TUNER_DEVICE_STATUS                = 0x07,  //Used by a tuner device to provide its status to the initiator of the CEC_OP_TC_GIVE_TUNER_STATUS message.
    CEC_OP_TC_TUNER_STEP_DEC                     = 0x06,  //Used to tune to next lowest service in a tuner's service list. Can be used for PIP.
    CEC_OP_TC_TUNER_STEP_INC                     = 0x05,  //Used to tune to next highest service in a tuner's service list. Can be used for PIP.
//---------Vendor Specific -------------------------
    CEC_OP_VS_DEVICE_VENDOR_ID                   = 0x87,  //Reports the vendor ID of this device.
    CEC_OP_VS_GIVE_VENDOR_ID                     = 0x8C,  //Requests the vendor ID from a device.
    CEC_OP_VS_VENDOR_COMMAND                     = 0x89,  //Allows vendor specific commands to be sent between two devices.
    CEC_OP_VS_VENDOR_COMMAND_WITH_ID             = 0xA0,  //Allows vendor specific commands to be sent between two devices or broadcast.
    CEC_OP_VS_VENDOR_RC_BUT_DOWN                 = 0x8A,  //Indicates that a remote control button has been depressed.
    CEC_OP_VS_VENDOR_RC_BUT_UP                   = 0x8B,  //Indicates that a remote control button(the last button pressed indicated by the CEC_OP_VS_VENDOR_RC_BUT_DOWN message) has been released.
//----- OSD Display --------------------------------
    CEC_OP_SET_OSD_STRING                        = 0x64,  //Used to send a text message to output on a TV.
//----- Device OSD Name Transfer  ------------------
    CEC_OP_OSDNT_GIVE_OSD_NAME                   = 0x46,  //Used to request the preferred OSD name of a device for use in menus associated with that device.
    CEC_OP_OSDNT_SET_OSD_NAME                    = 0x47,  //Used to set the preferred OSD name of a device for use in menus associated with that device.
//----- Device Menu Control ------------------------
    CEC_OP_DMC_MENU_REQUEST                      = 0x8D,  //A requeste from the TV for a device to show/remove a menu or to query if a device is currently showing a menu.
    CEC_OP_DMC_MENU_STATUS                       = 0x8E,  //Used to indicate to the TV that the device is showing/has removed a menu and requests the remote control keys to be passed through.
    CEC_OP_UI_PRESS                              = 0x44,  //Used to indicate that the user pressed a remote control button or switched from one remote control button to another. Can also be used as a command that is not directly initiated by the user.
    CEC_OP_UI_RELEASE                            = 0x45,  //Indicates that user released a remote control button (the last one indicated by the CEC_OP_UI_PRESS message). Also used after a command that is not directly initiated by the user.
//----- Power Status  ------------------------------
    CEC_OP_PS_GIVE_POWER_STATUS                  = 0x8F,  //Used to determine the current power status of a target device.
    CEC_OP_PS_REPORT_POWER_STATUS                = 0x90,  //Used to inform a requesting device of the current power status.
//----- General Protocal Message -------------------
    //----- Abort Message --------------------------
    CEC_OP_ABORT_MESSAGE                         = 0xFF,  //This message is reserved for testing purposes.
    //----- Feature Abort --------------------------
    CEC_OP_FEATURE_ABORT                         = 0x00,  //Used as a response to indicate that the device does not support the requested message type, or that it cannot execute it at the present time.
//----- System Audio Control -----------------------
    CEC_OP_SAC_GIVE_AUDIO_STATUS                 = 0x71,  //Requests an Amplifier to send its volume and mute status.
    CEC_OP_SAC_GIVE_SYSTEM_AUDIO_MODE_STATUS     = 0x7D,  //Requests the status of the system audio mode.
    CEC_OP_SAC_REPORT_AUDIO_STATUS               = 0x7A,  //Reports an Amplifier's volume and mute status.
    CEC_OP_SAC_SET_SYSTEM_AUDIO_MODE             = 0x72,  //Turns the System Audio Mode On of Off.
    CEC_OP_SAC_SYSTEM_AUDIO_MODE_REQUEST         = 0x70,  //A device implementing System Audio Control and which has volume control RC buttons requests to use System Audio Mode to the Amplifier.
    CEC_OP_SAC_SYSTEM_AUDIO_MODE_STATUS          = 0x7E,  //Reports the current status of the system audio mode.
    CEC_OP_SAC_REPORT_SHORT_AUDIO_DESCRIPTOR     = 0xA3,  //Report Audio Capability.
    CEC_OP_SAC_REQUEST_SHORT_AUDIO_DESCRIPTOR    = 0xA4,  //Request Audio Capability.
//----- System Audio Rate Control ------------------
    CEC_OP_SAC_SET_AUDIO_RATE                    = 0x9A,  //Used to control audio rate from Source Device.
//----- Audio Return Channel  Control --------------
    CEC_OP_ARC_INITIATE_ARC                      = 0xC0,  //Used by an ARC RX device to activate the ARC functionality in an ARC TX device.
    CEC_OP_ARC_REPORT_ARC_INITIATED              = 0xC1,  //Used by an ARC TX device to inidicate that its ARC functionality has been activated.
    CEC_OP_ARC_REPORT_ARC_TERMINATED             = 0xC2,  //Used by an ARC TX device to indicate that its ARC functionality has been deactivated.
    CEC_OP_ARC_REQUEST_ARC_INITIATION            = 0xC3,  //Used by an ARC TX device to request an ARC RX device to activate the ARC functionality in the ARC TX device.
    CEC_OP_ARC_REQUEST_ARC_TERMINATION           = 0xC4,  //Used by an ARC TX device to request an ARC RX device to deactivate the ARC functionality in the ARC TX device.
    CEC_OP_ARC_TERMINATED_ARC                    = 0xC5,  //Used by an ARC RX device to deactivate the ARC functionality in an ARC TX device.

} CEC_OpCode_e;



#ifdef __cplusplus
}
#endif

#endif///HWCEC_CEC_UTILS_H_