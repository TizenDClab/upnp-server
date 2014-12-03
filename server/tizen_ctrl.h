#ifndef UPNP_TIZE_NCTRLPT_H
#define UPNP_TIZE_NCTRLPT_H

/**************************************************************************
 *
 * Copyright (c) 2000-2003 Intel Corporation 
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are met: 
 *
 * - Redistributions of source code must retain the above copyright notice, 
 * this list of conditions and the following disclaimer. 
 * - Redistributions in binary form must reproduce the above copyright notice, 
 * this list of conditions and the following disclaimer in the documentation 
 * and/or other materials provided with the distribution. 
 * - Neither name of Intel Corporation nor the names of its contributors 
 * may be used to endorse or promote products derived from this software 
 * without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL INTEL OR 
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR 
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY 
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS 
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 **************************************************************************/

/*!
 * \addtogroup UpnpSamples
 *
 * @{
 *
 * \name Contro Point Sample API
 *
 * @{
 *
 * \file
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "sample_util.h"

#include "upnp.h"
#include "UpnpString.h"
#include "upnptools.h"

#include <signal.h>
#include <stdarg.h>
#include <stdio.h>

#define TIZEN_SERVICE_SERVCOUNT	2
#define TIZEN_SERVICE_CONTROL	0
#define TIZEN_SERVICE_PICTURE	1

#define TIZEN_CONTROL_VARCOUNT	3
#define TIZEN_CONTROL_POWER	0
#define TIZEN_CONTROL_CHANNEL	1
#define TIZEN_CONTROL_VOLUME	2

#define TIZEN_PICTURE_VARCOUNT	5
#define TIZEN_PICTURE_COLOR	0
#define TIZEN_PICTURE_TINT		1
#define TIZEN_PICTURE_CONTRAST	2
#define TIZEN_PICTURE_BRIGHTNESS	3

#define TIZEN_MAX_VAL_LEN		5

#define TIZEN_SUCCESS		0
#define TIZEN_ERROR		(-1)
#define TIZEN_WARNING		1

/* This should be the maximum VARCOUNT from above */
#define TIZEN_MAXVARS		TIZEN_PICTURE_VARCOUNT

extern const char *TizenServiceName[];
extern const char *TizenVarName[TIZEN_SERVICE_SERVCOUNT][TIZEN_MAXVARS];
extern char TizenVarCount[];

struct tizen_service {
    char ServiceId[NAME_SIZE];
    char ServiceType[NAME_SIZE];
    char *VariableStrVal[TIZEN_MAXVARS];
    char EventURL[NAME_SIZE];
    char ControlURL[NAME_SIZE];
    char SID[NAME_SIZE];
};

extern struct TizenDeviceNode *GlobalDeviceList;

struct TizenDevice {
    char UDN[250];
    char DescDocURL[250];
    char FriendlyName[250];
    char PresURL[250];
    int  AdvrTimeOut;
    struct tizen_service TizenService[TIZEN_SERVICE_SERVCOUNT];
};

struct TizenDeviceNode {
    struct TizenDevice device;
    struct TizenDeviceNode *next;
};

extern ithread_mutex_t DeviceListMutex;

extern UpnpClient_Handle ctrlpt_handle;

void	TizenCtrlPointPrintHelp(void);
int		TizenCtrlPointDeleteNode(struct TizenDeviceNode *);
int		TizenCtrlPointRemoveDevice(const char *);
int		TizenCtrlPointRemoveAll(void);
int		TizenCtrlPointRefresh(void);

int		TizenCtrlPointSendAction(int, int, const char *, const char **, char **, int);
int		TizenCtrlPointSendActionNumericArg(int devnum, int service, const char *actionName, const char *paramName, int paramValue);
int		TizenCtrlPointSendPowerOn(int devnum);
int		TizenCtrlPointSendPowerOff(int devnum);
int		TizenCtrlPointSendSetChannel(int, int);
int		TizenCtrlPointSendSetVolume(int, int);
int		TizenCtrlPointSendSetColor(int, int);
int		TizenCtrlPointSendSetTint(int, int);
int		TizenCtrlPointSendSetContrast(int, int);
int		TizenCtrlPointSendSetBrightness(int, int);

int		TizenCtrlPointGetVar(int, int, const char *);
int		TizenCtrlPointGetPower(int devnum);
int		TizenCtrlPointGetChannel(int);
int		TizenCtrlPointGetVolume(int);
int		TizenCtrlPointGetColor(int);
int		TizenCtrlPointGetTint(int);
int		TizenCtrlPointGetContrast(int);
int		TizenCtrlPointGetBrightness(int);

int		TizenCtrlPointGetDevice(int, struct TizenDeviceNode **);
int		TizenCtrlPointPrintList(void);
int		TizenCtrlPointPrintDevice(int);
void	TizenCtrlPointAddDevice(IXML_Document *, const char *, int); 
void    TizenCtrlPointHandleGetVar(const char *, const char *, const DOMString);

/*!
 * \brief Update a Tizen state table. Called when an event is received.
 *
 * Note: this function is NOT thread save. It must be called from another
 * function that has locked the global device list.
 **/
void TizenStateUpdate(
	/*! [in] The UDN of the parent device. */
	char *UDN,
	/*! [in] The service state table to update. */
	int Service,
	/*! [out] DOM document representing the XML received with the event. */
	IXML_Document *ChangedVariables,
	/*! [out] pointer to the state table for the Tizen  service to update. */
	char **State);

void	TizenCtrlPointHandleEvent(const char *, int, IXML_Document *); 
void	TizenCtrlPointHandleSubscribeUpdate(const char *, const Upnp_SID, int); 
int		TizenCtrlPointCallbackEventHandler(Upnp_EventType, void *, void *);

/*!
 * \brief Checks the advertisement each device in the global device list.
 *
 * If an advertisement expires, the device is removed from the list.
 *
 * If an advertisement is about to expire, a search request is sent for that
 * device.
 */
void TizenCtrlPointVerifyTimeouts(
	/*! [in] The increment to subtract from the timeouts each time the
	 * function is called. */
	int incr);

void	TizenCtrlPointPrintCommands(void);
void*	TizenCtrlPointCommandLoop(void *);
int		TizenCtrlPointStart(print_string printFunctionPtr, state_update updateFunctionPtr, int combo);
int		TizenCtrlPointStop(void);
int		TizenCtrlPointProcessCommand(char *cmdline);

/*!
 * \brief Print help info for this application.
 */
void TizenCtrlPointPrintShortHelp(void);

/*!
 * \brief Print long help info for this application.
 */
void TizenCtrlPointPrintLongHelp(void);

/*!
 * \briefPrint the list of valid command line commands to the user
 */
void TizenCtrlPointPrintCommands(void);

/*!
 * \brief Function that receives commands from the user at the command prompt
 * during the lifetime of the device, and calls the appropriate
 * functions for those commands.
 */
void *TizenCtrlPointCommandLoop(void *args);

/*!
 * \brief
 */
int TizenCtrlPointProcessCommand(char *cmdline);

#ifdef __cplusplus
};
#endif


/*! @} Device Sample */

/*! @} UpnpSamples */

#endif /* UPNP_TIZEN_CTRLPT_H */

