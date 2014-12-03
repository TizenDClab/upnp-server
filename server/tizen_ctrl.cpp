/*******************************************************************************
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
 ******************************************************************************/

/*!
 * \addtogroup UpnpSamples
 *
 * @{
 *
 * \name Control Point Sample Module
 *
 * @{
 *
 * \file
 */

#include "tizen_ctrl.h"

#include "upnp.h"

/*!
 * Mutex for protecting the global device list in a multi-threaded,
 * asynchronous environment. All functions should lock this mutex before
 * reading or writing the device list. 
 */
ithread_mutex_t DeviceListMutex;

UpnpClient_Handle ctrlpt_handle = -1;

/*! Device type for tizen device. */
const char TizenDeviceType[] = "urn:schemas-upnp-org:device:tizen:1";

/*! Service names.*/
const char *TizenServiceName[] = { "Control", "Picture" };

/*!
   Global arrays for storing variable names and counts for 
   TizenControl and TizenPicture services 
 */
const char *TizenVarName[TIZEN_SERVICE_SERVCOUNT][TIZEN_MAXVARS] = {
    {"Power", "Channel", "Volume", "", ""},
    {"Color", "Tint", "Contrast", "Brightness", "Text"}
};
char TizenVarCount[TIZEN_SERVICE_SERVCOUNT] =
    { TIZEN_CONTROL_VARCOUNT, TIZEN_PICTURE_VARCOUNT };

/*!
   Timeout to request during subscriptions 
 */
int default_timeout = 1801;

/*!
   The first node in the global device list, or NULL if empty 
 */
struct TizenDeviceNode *GlobalDeviceList = NULL;

/********************************************************************************
 * TizenCtrlPointDeleteNode
 *
 * Description: 
 *       Delete a device node from the global device list.  Note that this
 *       function is NOT thread safe, and should be called from another
 *       function that has already locked the global device list.
 *
 * Parameters:
 *   node -- The device node
 *
 ********************************************************************************/
int
TizenCtrlPointDeleteNode( struct TizenDeviceNode *node )
{
	int rc, service, var;

	if (NULL == node) {
		SampleUtil_Print
		    ("ERROR: TizenCtrlPointDeleteNode: Node is empty\n");
		return TIZEN_ERROR;
	}

	for (service = 0; service < TIZEN_SERVICE_SERVCOUNT; service++) {
		/*
		   If we have a valid control SID, then unsubscribe 
		 */
		if (strcmp(node->device.TizenService[service].SID, "") != 0) {
			rc = UpnpUnSubscribe(ctrlpt_handle,
					     node->device.TizenService[service].
					     SID);
			if (UPNP_E_SUCCESS == rc) {
				SampleUtil_Print
				    ("Unsubscribed from Tizen %s EventURL with SID=%s\n",
				     TizenServiceName[service],
				     node->device.TizenService[service].SID);
			} else {
				SampleUtil_Print
				    ("Error unsubscribing to Tizen %s EventURL -- %d\n",
				     TizenServiceName[service], rc);
			}
		}

		for (var = 0; var < TizenVarCount[service]; var++) {
			if (node->device.TizenService[service].VariableStrVal[var]) {
				free(node->device.
				     TizenService[service].VariableStrVal[var]);
			}
		}
	}

	/*Notify New Device Added */
	SampleUtil_StateUpdate(NULL, NULL, node->device.UDN, DEVICE_REMOVED);
	free(node);
	node = NULL;

	return TIZEN_SUCCESS;
}

/********************************************************************************
 * TizenCtrlPointRemoveDevice
 *
 * Description: 
 *       Remove a device from the global device list.
 *
 * Parameters:
 *   UDN -- The Unique Device Name for the device to remove
 *
 ********************************************************************************/
int TizenCtrlPointRemoveDevice(const char *UDN)
{
	struct TizenDeviceNode *curdevnode;
	struct TizenDeviceNode *prevdevnode;

	ithread_mutex_lock(&DeviceListMutex);

	curdevnode = GlobalDeviceList;
	if (!curdevnode) {
		SampleUtil_Print(
			"WARNING: TizenCtrlPointRemoveDevice: Device list empty\n");
	} else {
		if (0 == strcmp(curdevnode->device.UDN, UDN)) {
			GlobalDeviceList = curdevnode->next;
			TizenCtrlPointDeleteNode(curdevnode);
		} else {
			prevdevnode = curdevnode;
			curdevnode = curdevnode->next;
			while (curdevnode) {
				if (strcmp(curdevnode->device.UDN, UDN) == 0) {
					prevdevnode->next = curdevnode->next;
					TizenCtrlPointDeleteNode(curdevnode);
					break;
				}
				prevdevnode = curdevnode;
				curdevnode = curdevnode->next;
			}
		}
	}

	ithread_mutex_unlock(&DeviceListMutex);

	return TIZEN_SUCCESS;
}

/********************************************************************************
 * TizenCtrlPointRemoveAll
 *
 * Description: 
 *       Remove all devices from the global device list.
 *
 * Parameters:
 *   None
 *
 ********************************************************************************/
int TizenCtrlPointRemoveAll(void)
{
	struct TizenDeviceNode *curdevnode, *next;

	ithread_mutex_lock(&DeviceListMutex);

	curdevnode = GlobalDeviceList;
	GlobalDeviceList = NULL;

	while (curdevnode) {
		next = curdevnode->next;
		TizenCtrlPointDeleteNode(curdevnode);
		curdevnode = next;
	}

	ithread_mutex_unlock(&DeviceListMutex);

	return TIZEN_SUCCESS;
}

/********************************************************************************
 * TizenCtrlPointRefresh
 *
 * Description: 
 *       Clear the current global device list and issue new search
 *	 requests to build it up again from scratch.
 *
 * Parameters:
 *   None
 *
 ********************************************************************************/
int TizenCtrlPointRefresh(void)
{
	int rc;

	TizenCtrlPointRemoveAll();
	/* Search for all devices of type tizendevice version 1,
	 * waiting for up to 5 seconds for the response */
	rc = UpnpSearchAsync(ctrlpt_handle, 5, TizenDeviceType, NULL);
	if (UPNP_E_SUCCESS != rc) {
		SampleUtil_Print("Error sending search request%d\n", rc);

		return TIZEN_ERROR;
	}

	return TIZEN_SUCCESS;
}

/********************************************************************************
 * TizenCtrlPointGetVar
 *
 * Description: 
 *       Send a GetVar request to the specified service of a device.
 *
 * Parameters:
 *   service -- The service
 *   devnum -- The number of the device (order in the list,
 *             starting with 1)
 *   varname -- The name of the variable to request.
 *
 ********************************************************************************/
int TizenCtrlPointGetVar(int service, int devnum, const char *varname)
{
	struct TizenDeviceNode *devnode;
	int rc;

	ithread_mutex_lock(&DeviceListMutex);

	rc = TizenCtrlPointGetDevice(devnum, &devnode);

	if (TIZEN_SUCCESS == rc) {
		rc = UpnpGetServiceVarStatusAsync(
			ctrlpt_handle,
			devnode->device.TizenService[service].ControlURL,
			varname,
			TizenCtrlPointCallbackEventHandler,
			NULL);
		if (rc != UPNP_E_SUCCESS) {
			SampleUtil_Print(
				"Error in UpnpGetServiceVarStatusAsync -- %d\n",
				rc);
			rc = TIZEN_ERROR;
		}
	}

	ithread_mutex_unlock(&DeviceListMutex);

	return rc;
}

int TizenCtrlPointGetPower(int devnum)
{
	return TizenCtrlPointGetVar(TIZEN_SERVICE_CONTROL, devnum, "Power");
}

int TizenCtrlPointGetChannel(int devnum)
{
	return TizenCtrlPointGetVar(TIZEN_SERVICE_CONTROL, devnum, "Channel");
}

int TizenCtrlPointGetVolume(int devnum)
{
	return TizenCtrlPointGetVar(TIZEN_SERVICE_CONTROL, devnum, "Volume");
}

int TizenCtrlPointGetColor(int devnum)
{
	return TizenCtrlPointGetVar(TIZEN_SERVICE_PICTURE, devnum, "Color");
}

int TizenCtrlPointGetTint(int devnum)
{
	return TizenCtrlPointGetVar(TIZEN_SERVICE_PICTURE, devnum, "Tint");
}

int TizenCtrlPointGetContrast(int devnum)
{
	return TizenCtrlPointGetVar(TIZEN_SERVICE_PICTURE, devnum, "Contrast");
}

int TizenCtrlPointGetBrightness(int devnum)
{
	return TizenCtrlPointGetVar(TIZEN_SERVICE_PICTURE, devnum, "Brightness");
}

/********************************************************************************
 * TizenCtrlPointSendAction
 *
 * Description: 
 *       Send an Action request to the specified service of a device.
 *
 * Parameters:
 *   service -- The service
 *   devnum -- The number of the device (order in the list,
 *             starting with 1)
 *   actionname -- The name of the action.
 *   param_name -- An array of parameter names
 *   param_val -- The corresponding parameter values
 *   param_count -- The number of parameters
 *
 ********************************************************************************/
int TizenCtrlPointSendAction(
	int service,
	int devnum,
	const char *actionname,
	const char **param_name,
	char **param_val,
	int param_count)
{
	struct TizenDeviceNode *devnode;
	IXML_Document *actionNode = NULL;
	int rc = TIZEN_SUCCESS;
	int param;

	ithread_mutex_lock(&DeviceListMutex);
	rc = TizenCtrlPointGetDevice(devnum, &devnode);
	if (TIZEN_SUCCESS == rc) {
		if (0 == param_count) {
			actionNode =
			    UpnpMakeAction(actionname, TizenServiceType[service],
					   0, NULL);
		} else {
			for (param = 0; param < param_count; param++) {
				if (UpnpAddToAction
				    (&actionNode, actionname,
				     TizenServiceType[service], param_name[param],
				     param_val[param]) != UPNP_E_SUCCESS) {
					SampleUtil_Print
					    ("ERROR: TizenCtrlPointSendAction: Trying to add action param\n");
					/*return -1; // TBD - BAD! leaves mutex locked */
				}
			}
		}
		

		rc = UpnpSendActionAsync(ctrlpt_handle,
					 devnode->device.
					 TizenService[service].ControlURL,
					 TizenServiceType[service], NULL,
					 actionNode,
					 TizenCtrlPointCallbackEventHandler, NULL);

		if (rc != UPNP_E_SUCCESS) {
			SampleUtil_Print("Error in UpnpSendActionAsync -- %d\n",
					 rc);
			rc = TIZEN_ERROR;
		}
	}

	ithread_mutex_unlock(&DeviceListMutex);

	if (actionNode)
		ixmlDocument_free(actionNode);
	

	return rc;
}

/********************************************************************************
 * TizenCtrlPointSendActionNumericArg
 *
 * Description:Send an action with one argument to a device in the global device list.
 *
 * Parameters:
 *   devnum -- The number of the device (order in the list, starting with 1)
 *   service -- TIZEN_SERVICE_CONTROL or TIZEN_SERVICE_PICTURE
 *   actionName -- The device action, i.e., "SetChannel"
 *   paramName -- The name of the parameter that is being passed
 *   paramValue -- Actual value of the parameter being passed
 *
 ********************************************************************************/
int TizenCtrlPointSendActionNumericArg(int devnum, int service,
	const char *actionName, const char *paramName, int paramValue)
{
	char param_val_a[50];
	char *param_val = param_val_a;

	sprintf(param_val_a, "%d", paramValue);
	return TizenCtrlPointSendAction(
		service, devnum, actionName, &paramName,
		&param_val, 1);
}

int TizenCtrlPointSendActionTextArg(int devnum, int service,
	const char *actionName, const char *paramName, char *paramText)
{
	char param_val_a[50];
	char *param_val = param_val_a;

	sprintf(param_val_a, "%s", paramText);
//	printf("Service:%d, devnum:%d actionName:%s paramName:%s param_val:%s \n", 
//			service, devnum, actionName, paramName, &param_val);
	return TizenCtrlPointSendAction(
		service, devnum, actionName, &paramName,
		&param_val, 1);
}

int TizenCtrlPointSendPowerOn(int devnum)
{
	return TizenCtrlPointSendAction(
		TIZEN_SERVICE_CONTROL, devnum, "PowerOn", NULL, NULL, 0);
}

int TizenCtrlPointSendPowerOff(int devnum)
{
	return TizenCtrlPointSendAction(
		TIZEN_SERVICE_CONTROL, devnum, "PowerOff", NULL, NULL, 0);
}

int TizenCtrlPointSendSetChannel(int devnum, int channel)
{
	return TizenCtrlPointSendActionNumericArg(
		devnum, TIZEN_SERVICE_CONTROL, "SetChannel", "Channel", channel);
}

int TizenCtrlPointSendSetVolume(int devnum, int volume)
{
	return TizenCtrlPointSendActionNumericArg(
		devnum, TIZEN_SERVICE_CONTROL, "SetVolume", "Volume", volume);
}

int TizenCtrlPointSendSetColor(int devnum, int color)
{
	return TizenCtrlPointSendActionNumericArg(
		devnum, TIZEN_SERVICE_PICTURE, "SetColor", "Color", color);
}

int TizenCtrlPointSendSetTint(int devnum, int tint)
{
	return TizenCtrlPointSendActionNumericArg(
		devnum, TIZEN_SERVICE_PICTURE, "SetTint", "Tint", tint);
}

int TizenCtrlPointSendSetContrast(int devnum, int contrast)
{
	return TizenCtrlPointSendActionNumericArg(
		devnum, TIZEN_SERVICE_PICTURE, "SetContrast", "Contrast",
		contrast);
}

int TizenCtrlPointSendSetBrightness(int devnum, int brightness)
{
	return TizenCtrlPointSendActionNumericArg(
		devnum, TIZEN_SERVICE_PICTURE, "SetBrightness", "Brightness",
		brightness);
}

int TizenCtrlPointSendText(int devnum, char *text)
{
	return TizenCtrlPointSendActionTextArg(
		devnum, TIZEN_SERVICE_PICTURE, "SendText", "Text",
		text);
}

/********************************************************************************
 * TizenCtrlPointGetDevice
 *
 * Description: 
 *       Given a list number, returns the pointer to the device
 *       node at that position in the global device list.  Note
 *       that this function is not thread safe.  It must be called 
 *       from a function that has locked the global device list.
 *
 * Parameters:
 *   devnum -- The number of the device (order in the list,
 *             starting with 1)
 *   devnode -- The output device node pointer
 *
 ********************************************************************************/
int TizenCtrlPointGetDevice(int devnum, struct TizenDeviceNode **devnode)
{
	int count = devnum;
	struct TizenDeviceNode *tmpdevnode = NULL;

	if (count)
		tmpdevnode = GlobalDeviceList;
	while (--count && tmpdevnode) {
		tmpdevnode = tmpdevnode->next;
	}
	if (!tmpdevnode) {
		SampleUtil_Print("Error finding TizenDevice number -- %d\n",
				 devnum);
		return TIZEN_ERROR;
	}
	*devnode = tmpdevnode;

	return TIZEN_SUCCESS;
}

/********************************************************************************
 * TizenCtrlPointPrintList
 *
 * Description: 
 *       Print the universal device names for each device in the global device list
 *
 * Parameters:
 *   None
 *
 ********************************************************************************/
int TizenCtrlPointPrintList()
{
	struct TizenDeviceNode *tmpdevnode;
	int i = 0;

	ithread_mutex_lock(&DeviceListMutex);

	SampleUtil_Print("TizenCtrlPointPrintList:\n");
	tmpdevnode = GlobalDeviceList;
	while (tmpdevnode) {
		SampleUtil_Print(" %3d -- %s\n", ++i, tmpdevnode->device.UDN);
		tmpdevnode = tmpdevnode->next;
	}
	SampleUtil_Print("\n");
	ithread_mutex_unlock(&DeviceListMutex);

	return TIZEN_SUCCESS;
}

/********************************************************************************
 * TizenCtrlPointPrintDevice
 *
 * Description: 
 *       Print the identifiers and state table for a device from
 *       the global device list.
 *
 * Parameters:
 *   devnum -- The number of the device (order in the list,
 *             starting with 1)
 *
 ********************************************************************************/
int TizenCtrlPointPrintDevice(int devnum)
{
	struct TizenDeviceNode *tmpdevnode;
	int i = 0, service, var;
	char spacer[15];

	if (devnum <= 0) {
		SampleUtil_Print(
			"Error in TizenCtrlPointPrintDevice: "
			"invalid devnum = %d\n",
			devnum);
		return TIZEN_ERROR;
	}

	ithread_mutex_lock(&DeviceListMutex);

	SampleUtil_Print("TizenCtrlPointPrintDevice:\n");
	tmpdevnode = GlobalDeviceList;
	while (tmpdevnode) {
		i++;
		if (i == devnum)
			break;
		tmpdevnode = tmpdevnode->next;
	}
	if (!tmpdevnode) {
		SampleUtil_Print(
			"Error in TizenCtrlPointPrintDevice: "
			"invalid devnum = %d  --  actual device count = %d\n",
			devnum, i);
	} else {
		SampleUtil_Print(
			"  TizenDevice -- %d\n"
			"    |                  \n"
			"    +- UDN        = %s\n"
			"    +- DescDocURL     = %s\n"
			"    +- FriendlyName   = %s\n"
			"    +- PresURL        = %s\n"
			"    +- Adver. TimeOut = %d\n",
			devnum,
			tmpdevnode->device.UDN,
			tmpdevnode->device.DescDocURL,
			tmpdevnode->device.FriendlyName,
			tmpdevnode->device.PresURL,
			tmpdevnode->device.AdvrTimeOut);
		for (service = 0; service < TIZEN_SERVICE_SERVCOUNT; service++) {
			if (service < TIZEN_SERVICE_SERVCOUNT - 1)
				sprintf(spacer, "    |    ");
			else
				sprintf(spacer, "         ");
			SampleUtil_Print(
				"    |                  \n"
				"    +- Tizen %s Service\n"
				"%s+- ServiceId       = %s\n"
				"%s+- ServiceType     = %s\n"
				"%s+- EventURL        = %s\n"
				"%s+- ControlURL      = %s\n"
				"%s+- SID             = %s\n"
				"%s+- ServiceStateTable\n",
				TizenServiceName[service],
				spacer,
				tmpdevnode->device.TizenService[service].ServiceId,
				spacer,
				tmpdevnode->device.TizenService[service].ServiceType,
				spacer,
				tmpdevnode->device.TizenService[service].EventURL,
				spacer,
				tmpdevnode->device.TizenService[service].ControlURL,
				spacer,
				tmpdevnode->device.TizenService[service].SID,
				spacer);
			for (var = 0; var < TizenVarCount[service]; var++) {
				SampleUtil_Print(
					"%s     +- %-10s = %s\n",
					spacer,
					TizenVarName[service][var],
					tmpdevnode->device.TizenService[service].VariableStrVal[var]);
			}
		}
	}
	SampleUtil_Print("\n");
	ithread_mutex_unlock(&DeviceListMutex);

	return TIZEN_SUCCESS;
}

/********************************************************************************
 * TizenCtrlPointAddDevice
 *
 * Description: 
 *       If the device is not already included in the global device list,
 *       add it.  Otherwise, update its advertisement expiration timeout.
 *
 * Parameters:
 *   DescDoc -- The description document for the device
 *   location -- The location of the description document URL
 *   expires -- The expiration time for this advertisement
 *
 ********************************************************************************/
void TizenCtrlPointAddDevice(
	IXML_Document *DescDoc,
	const char *location,
	int expires)
{
	char *deviceType = NULL;
	char *friendlyName = NULL;
	char presURL[200];
	char *baseURL = NULL;
	char *relURL = NULL;
	char *UDN = NULL;
	char *deviceTizen = NULL;
	char *serviceId[TIZEN_SERVICE_SERVCOUNT] = { NULL, NULL };
	char *eventURL[TIZEN_SERVICE_SERVCOUNT] = { NULL, NULL };
	char *controlURL[TIZEN_SERVICE_SERVCOUNT] = { NULL, NULL };
	Upnp_SID eventSID[TIZEN_SERVICE_SERVCOUNT];
	int TimeOut[TIZEN_SERVICE_SERVCOUNT] = {
		default_timeout,
		default_timeout
	};
	struct TizenDeviceNode *deviceNode;
	struct TizenDeviceNode *tmpdevnode;
	int ret = 1;
	int found = 0;
	int service;
	int var;

	ithread_mutex_lock(&DeviceListMutex);

	/* Read key elements from description document */
	deviceTizen = SampleUtil_GetFirstDocumentItem(DescDoc, "modelName");
	UDN = SampleUtil_GetFirstDocumentItem(DescDoc, "UDN");
	deviceType = SampleUtil_GetFirstDocumentItem(DescDoc, "deviceType");
	friendlyName = SampleUtil_GetFirstDocumentItem(DescDoc, "friendlyName");
	//baseURL = SampleUtil_GetFirstDocumentItem(DescDoc, "URLBase");
	relURL = SampleUtil_GetFirstDocumentItem(DescDoc, "presentationURL");

	if(!deviceTizen || strcmp(deviceTizen, "Tizen")) {
		goto __finish_add_device;
	}
	SampleUtil_Print("UDN        = %s\n", UDN);
	SampleUtil_Print("deviceType = %s\n", deviceType);

	ret = UpnpResolveURL((baseURL ? baseURL : location), relURL, presURL);

	if (UPNP_E_SUCCESS != ret)
		SampleUtil_Print("Error generating presURL from %s + %s\n",
				 baseURL, relURL);

	if (strcmp(deviceType, TizenDeviceType) == 0) {
		SampleUtil_Print("Found Tizen device\n");

		/* Check if this device is already in the list */
		tmpdevnode = GlobalDeviceList;
		while (tmpdevnode) {
			if (strcmp(tmpdevnode->device.UDN, UDN) == 0) {
				found = 1;
				break;
			}
			tmpdevnode = tmpdevnode->next;
		}

		if (found) {
			/* The device is already there, so just update  */
			/* the advertisement timeout field */
			tmpdevnode->device.AdvrTimeOut = expires;
		} else {
			for (service = 0; service < TIZEN_SERVICE_SERVCOUNT;
			     service++) {
				if (SampleUtil_FindAndParseService
				    (DescDoc, location, TizenServiceType[service],
				     &serviceId[service], &eventURL[service],
				     &controlURL[service])) {
					SampleUtil_Print
					    ("Subscribing to EventURL %s...\n",
					     eventURL[service]);
					ret =
					    UpnpSubscribe(ctrlpt_handle,
							  eventURL[service],
							  &TimeOut[service],
							  eventSID[service]);
					if (ret == UPNP_E_SUCCESS) {
						SampleUtil_Print
						    ("Subscribed to EventURL with SID=%s\n",
						     eventSID[service]);
					} else {
						SampleUtil_Print
						    ("Error Subscribing to EventURL -- %d\n",
						     ret);
						strcpy(eventSID[service], "");
					}
				} else {
					SampleUtil_Print
					    ("Error: Could not find Service: %s\n",
					     TizenServiceType[service]);
				}
			}
			/* Create a new device node */
			deviceNode =
			    (struct TizenDeviceNode *)
			    malloc(sizeof(struct TizenDeviceNode));
			strcpy(deviceNode->device.UDN, UDN);
			strcpy(deviceNode->device.DescDocURL, location);
			strcpy(deviceNode->device.FriendlyName, friendlyName);
			strcpy(deviceNode->device.PresURL, presURL);
			deviceNode->device.AdvrTimeOut = expires;
			for (service = 0; service < TIZEN_SERVICE_SERVCOUNT;
			     service++) {
				if (serviceId[service] == NULL) {
					/* not found */
					continue;
				}
				strcpy(deviceNode->device.TizenService[service].
				       ServiceId, serviceId[service]);
				strcpy(deviceNode->device.TizenService[service].
				       ServiceType, TizenServiceType[service]);
				strcpy(deviceNode->device.TizenService[service].
				       ControlURL, controlURL[service]);
				strcpy(deviceNode->device.TizenService[service].
				       EventURL, eventURL[service]);
				strcpy(deviceNode->device.TizenService[service].
				       SID, eventSID[service]);
				for (var = 0; var < TizenVarCount[service]; var++) {
					deviceNode->device.
					    TizenService[service].VariableStrVal
					    [var] =
					    (char *)malloc(TIZEN_MAX_VAL_LEN);
					strcpy(deviceNode->device.
					       TizenService[service].VariableStrVal
					       [var], "");
				}
			}
			printf("------------------------------------------\n");
			deviceNode->next = NULL;
			/* Insert the new device node in the list */
			if ((tmpdevnode = GlobalDeviceList)) {
				while (tmpdevnode) {
					if (tmpdevnode->next) {
						tmpdevnode = tmpdevnode->next;
					} else {
						tmpdevnode->next = deviceNode;
						break;
					}
				}
			} else {
				GlobalDeviceList = deviceNode;
			}
			/*Notify New Device Added */
			SampleUtil_StateUpdate(NULL, NULL,
					       deviceNode->device.UDN,
					       DEVICE_ADDED);
		}
	}

__finish_add_device :

	ithread_mutex_unlock(&DeviceListMutex);

	if (deviceTizen)
		free(deviceTizen);
	if (deviceType)
		free(deviceType);
	if (friendlyName)
		free(friendlyName);
	if (UDN)
		free(UDN);
	if (baseURL)
		free(baseURL);
	if (relURL)
		free(relURL);
	for (service = 0; service < TIZEN_SERVICE_SERVCOUNT; service++) {
		if (serviceId[service])
			free(serviceId[service]);
		if (controlURL[service])
			free(controlURL[service]);
		if (eventURL[service])
			free(eventURL[service]);
	}
}

void TizenStateUpdate(char *UDN, int Service, IXML_Document *ChangedVariables,
		   char **State)
{
	IXML_NodeList *properties;
	IXML_NodeList *variables;
	IXML_Element *property;
	IXML_Element *variable;
	long unsigned int length;
	long unsigned int length1;
	long unsigned int i;
	int j;
	char *tmpstate = NULL;

	SampleUtil_Print("Tizen State Update (service %d):\n", Service);
	/* Find all of the e:property tags in the document */
	properties = ixmlDocument_getElementsByTagName(ChangedVariables,
		"e:property");
	if (properties) {
		length = ixmlNodeList_length(properties);
		for (i = 0; i < length; i++) {
			/* Loop through each property change found */
			property = (IXML_Element *)ixmlNodeList_item(
				properties, i);
			/* For each variable name in the state table,
			 * check if this is a corresponding property change */
			for (j = 0; j < TizenVarCount[Service]; j++) {
				variables = ixmlElement_getElementsByTagName(
					property, TizenVarName[Service][j]);
				/* If a match is found, extract 
				 * the value, and update the state table */
				if (variables) {
					length1 = ixmlNodeList_length(variables);
					if (length1) {
						variable = (IXML_Element *)
							ixmlNodeList_item(variables, 0);
						tmpstate =
						    SampleUtil_GetElementValue(variable);
						if (tmpstate) {
							strcpy(State[j], tmpstate);
							SampleUtil_Print(
								" Variable Name: %s New Value:'%s'\n",
								TizenVarName[Service][j], State[j]);
						}
						if (tmpstate)
							free(tmpstate);
						tmpstate = NULL;
					}
					ixmlNodeList_free(variables);
					variables = NULL;
				}
			}
		}
		ixmlNodeList_free(properties);
	}
	return;
	UDN = UDN;
}

/********************************************************************************
 * TizenCtrlPointHandleEvent
 *
 * Description: 
 *       Handle a UPnP event that was received.  Process the event and update
 *       the appropriate service state table.
 *
 * Parameters:
 *   sid -- The subscription id for the event
 *   eventkey -- The eventkey number for the event
 *   changes -- The DOM document representing the changes
 *
 ********************************************************************************/
void TizenCtrlPointHandleEvent(
	const char *sid,
	int evntkey,
	IXML_Document *changes)
{
	struct TizenDeviceNode *tmpdevnode;
	int service;

	ithread_mutex_lock(&DeviceListMutex);

	tmpdevnode = GlobalDeviceList;
	while (tmpdevnode) {
		for (service = 0; service < TIZEN_SERVICE_SERVCOUNT; ++service) {
			if (strcmp(tmpdevnode->device.TizenService[service].SID, sid) ==  0) {
				SampleUtil_Print("Received Tizen %s Event: %d for SID %s\n",
					TizenServiceName[service],
					evntkey,
					sid);
				TizenStateUpdate(
					tmpdevnode->device.UDN,
					service,
					changes,
					(char **)&tmpdevnode->device.TizenService[service].VariableStrVal);
				break;
			}
		}
		tmpdevnode = tmpdevnode->next;
	}

	ithread_mutex_unlock(&DeviceListMutex);
}

/********************************************************************************
 * TizenCtrlPointHandleSubscribeUpdate
 *
 * Description: 
 *       Handle a UPnP subscription update that was received.  Find the 
 *       service the update belongs to, and update its subscription
 *       timeout.
 *
 * Parameters:
 *   eventURL -- The event URL for the subscription
 *   sid -- The subscription id for the subscription
 *   timeout  -- The new timeout for the subscription
 *
 ********************************************************************************/
void TizenCtrlPointHandleSubscribeUpdate(
	const char *eventURL,
	const Upnp_SID sid,
	int timeout)
{
	struct TizenDeviceNode *tmpdevnode;
	int service;

	ithread_mutex_lock(&DeviceListMutex);

	tmpdevnode = GlobalDeviceList;
	while (tmpdevnode) {
		for (service = 0; service < TIZEN_SERVICE_SERVCOUNT; service++) {
			if (strcmp
			    (tmpdevnode->device.TizenService[service].EventURL,
			     eventURL) == 0) {
				SampleUtil_Print
				    ("Received Tizen %s Event Renewal for eventURL %s\n",
				     TizenServiceName[service], eventURL);
				strcpy(tmpdevnode->device.TizenService[service].
				       SID, sid);
				break;
			}
		}

		tmpdevnode = tmpdevnode->next;
	}

	ithread_mutex_unlock(&DeviceListMutex);

	return;
	timeout = timeout;
}

void TizenCtrlPointHandleGetVar(
	const char *controlURL,
	const char *varName,
	const DOMString varValue)
{

	struct TizenDeviceNode *tmpdevnode;
	int service;

	ithread_mutex_lock(&DeviceListMutex);

	tmpdevnode = GlobalDeviceList;
	while (tmpdevnode) {
		for (service = 0; service < TIZEN_SERVICE_SERVCOUNT; service++) {
			if (strcmp
			    (tmpdevnode->device.TizenService[service].ControlURL,
			     controlURL) == 0) {
				SampleUtil_StateUpdate(varName, varValue,
						       tmpdevnode->device.UDN,
						       GET_VAR_COMPLETE);
				break;
			}
		}
		tmpdevnode = tmpdevnode->next;
	}

	ithread_mutex_unlock(&DeviceListMutex);
}

/********************************************************************************
 * TizenCtrlPointCallbackEventHandler
 *
 * Description: 
 *       The callback handler registered with the SDK while registering
 *       the control point.  Detects the type of callback, and passes the 
 *       request on to the appropriate function.
 *
 * Parameters:
 *   EventType -- The type of callback event
 *   Event -- Data structure containing event data
 *   Cookie -- Optional data specified during callback registration
 *
 ********************************************************************************/
int TizenCtrlPointCallbackEventHandler(Upnp_EventType EventType, void *Event, void *Cookie)
{
	/*int errCode = 0;*/

#ifndef TIZEN
	SampleUtil_PrintEvent(EventType, Event);
#endif
	switch ( EventType ) {
	/* SSDP Stuff */
	case UPNP_DISCOVERY_ADVERTISEMENT_ALIVE:
	case UPNP_DISCOVERY_SEARCH_RESULT: {
		struct Upnp_Discovery *d_event = (struct Upnp_Discovery *)Event;
		IXML_Document *DescDoc = NULL;
		int ret;

		if (d_event->ErrCode != UPNP_E_SUCCESS) {
			SampleUtil_Print("Error in Discovery Callback -- %d\n",
				d_event->ErrCode);
		}
		ret = UpnpDownloadXmlDoc(d_event->Location, &DescDoc);
		if (ret != UPNP_E_SUCCESS) {
			SampleUtil_Print("Error obtaining device description from %s -- error = %d\n",
				d_event->Location, ret);
		} else {
			TizenCtrlPointAddDevice(
				DescDoc, d_event->Location, d_event->Expires);
		}
		if (DescDoc) {
			ixmlDocument_free(DescDoc);
		}
		TizenCtrlPointPrintList();
		break;
	}
	case UPNP_DISCOVERY_SEARCH_TIMEOUT:
		/* Nothing to do here... */
		break;
	case UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE: {
		struct Upnp_Discovery *d_event = (struct Upnp_Discovery *)Event;

		if (d_event->ErrCode != UPNP_E_SUCCESS) {
			SampleUtil_Print("Error in Discovery ByeBye Callback -- %d\n",
					d_event->ErrCode);
		}
		SampleUtil_Print("Received ByeBye for Device: %s\n", d_event->DeviceId);
		TizenCtrlPointRemoveDevice(d_event->DeviceId);
		SampleUtil_Print("After byebye:\n");
		TizenCtrlPointPrintList();
		break;
	}
	/* SOAP Stuff */
	case UPNP_CONTROL_ACTION_COMPLETE: {
		struct Upnp_Action_Complete *a_event = (struct Upnp_Action_Complete *)Event;

		if (a_event->ErrCode != UPNP_E_SUCCESS) {
			SampleUtil_Print("Error in  Action Complete Callback -- %d\n",
					a_event->ErrCode);
		}
		/* No need for any processing here, just print out results.
		 * Service state table updates are handled by events. */
		break;
	}
	case UPNP_CONTROL_GET_VAR_COMPLETE: {
		struct Upnp_State_Var_Complete *sv_event = (struct Upnp_State_Var_Complete *)Event;

		if (sv_event->ErrCode != UPNP_E_SUCCESS) {
			SampleUtil_Print("Error in Get Var Complete Callback -- %d\n",
					sv_event->ErrCode);
		} else {
			TizenCtrlPointHandleGetVar(
				sv_event->CtrlUrl,
				sv_event->StateVarName,
				sv_event->CurrentVal);
		}
		break;
	}
	/* GENA Stuff */
	case UPNP_EVENT_RECEIVED: {
		struct Upnp_Event *e_event = (struct Upnp_Event *)Event;

		TizenCtrlPointHandleEvent(
			e_event->Sid,
			e_event->EventKey,
			e_event->ChangedVariables);
		break;
	}
	case UPNP_EVENT_SUBSCRIBE_COMPLETE:
	case UPNP_EVENT_UNSUBSCRIBE_COMPLETE:
	case UPNP_EVENT_RENEWAL_COMPLETE: {
		struct Upnp_Event_Subscribe *es_event = (struct Upnp_Event_Subscribe *)Event;

		if (es_event->ErrCode != UPNP_E_SUCCESS) {
			SampleUtil_Print("Error in Event Subscribe Callback -- %d\n",
					es_event->ErrCode);
		} else {
			TizenCtrlPointHandleSubscribeUpdate(
				es_event->PublisherUrl,
				es_event->Sid,
				es_event->TimeOut);
		}
		break;
	}
	case UPNP_EVENT_AUTORENEWAL_FAILED:
	case UPNP_EVENT_SUBSCRIPTION_EXPIRED: {
		struct Upnp_Event_Subscribe *es_event = (struct Upnp_Event_Subscribe *)Event;
		int TimeOut = default_timeout;
		Upnp_SID newSID;
		int ret;

		ret = UpnpSubscribe(
			ctrlpt_handle,
			es_event->PublisherUrl,
			&TimeOut,
			newSID);
		if (ret == UPNP_E_SUCCESS) {
			SampleUtil_Print("Subscribed to EventURL with SID=%s\n", newSID);
			TizenCtrlPointHandleSubscribeUpdate(
				es_event->PublisherUrl,
				newSID,
				TimeOut);
		} else {
			SampleUtil_Print("Error Subscribing to EventURL -- %d\n", ret);
		}
		break;
	}
	/* ignore these cases, since this is not a device */
	case UPNP_EVENT_SUBSCRIPTION_REQUEST:
	case UPNP_CONTROL_GET_VAR_REQUEST:
	case UPNP_CONTROL_ACTION_REQUEST:
		break;
	}

	return 0;
	Cookie = Cookie;
}

void TizenCtrlPointVerifyTimeouts(int incr)
{
	struct TizenDeviceNode *prevdevnode;
	struct TizenDeviceNode *curdevnode;
	int ret;

	ithread_mutex_lock(&DeviceListMutex);

	prevdevnode = NULL;
	curdevnode = GlobalDeviceList;
	while (curdevnode) {
		curdevnode->device.AdvrTimeOut -= incr;
		/*SampleUtil_Print("Advertisement Timeout: %d\n", curdevnode->device.AdvrTimeOut); */
		if (curdevnode->device.AdvrTimeOut <= 0) {
			/* This advertisement has expired, so we should remove the device
			 * from the list */
			if (GlobalDeviceList == curdevnode)
				GlobalDeviceList = curdevnode->next;
			else
				prevdevnode->next = curdevnode->next;
			TizenCtrlPointDeleteNode(curdevnode);
			if (prevdevnode)
				curdevnode = prevdevnode->next;
			else
				curdevnode = GlobalDeviceList;
		} else {
			if (curdevnode->device.AdvrTimeOut < 2 * incr) {
				/* This advertisement is about to expire, so
				 * send out a search request for this device
				 * UDN to try to renew */
				ret = UpnpSearchAsync(ctrlpt_handle, incr,
						      curdevnode->device.UDN,
						      NULL);
				if (ret != UPNP_E_SUCCESS)
					SampleUtil_Print
					    ("Error sending search request for Device UDN: %s -- err = %d\n",
					     curdevnode->device.UDN, ret);
			}
			prevdevnode = curdevnode;
			curdevnode = curdevnode->next;
		}
	}

	ithread_mutex_unlock(&DeviceListMutex);
}

/*!
 * \brief Function that runs in its own thread and monitors advertisement
 * and subscription timeouts for devices in the global device list.
 */
static int TizenCtrlPointTimerLoopRun = 1;
void *TizenCtrlPointTimerLoop(void *args)
{
	/* how often to verify the timeouts, in seconds */
	int incr = 30;

	while (TizenCtrlPointTimerLoopRun) {
		isleep((unsigned int)incr);
		TizenCtrlPointVerifyTimeouts(incr);
	}

	return NULL;
	args = args;
}

/*!
 * \brief Call this function to initialize the UPnP library and start the TV
 * Control Point.  This function creates a timer thread and provides a
 * callback handler to process any UPnP events that are received.
 *
 * \return TIZEN_SUCCESS if everything went well, else TIZEN_ERROR.
 */
int TizenCtrlPointStart(print_string printFunctionPtr, state_update updateFunctionPtr, int combo)
{
	ithread_t timer_thread;
	int rc;
	unsigned short port = 0;
	char *ip_address = NULL;
	
	FILE * fp = fopen("server_system.txt", "w+");

	SampleUtil_Initialize(printFunctionPtr);
	SampleUtil_RegisterUpdateFunction(updateFunctionPtr);

	ithread_mutex_init(&DeviceListMutex, 0);

	SampleUtil_Print("Initializing UPnP Sdk with\n"
			 "\tipaddress = %s port = %u\n",
			 ip_address ? ip_address : "{NULL}", port);

	rc = UpnpInit(ip_address, port);
	if (rc != UPNP_E_SUCCESS) {
		SampleUtil_Print("WinCEStart: UpnpInit() Error: %d\n", rc);
		if (!combo) {
			UpnpFinish();

			return TIZEN_ERROR;
		}
	}
	if (!ip_address) {
		ip_address = UpnpGetServerIpAddress();
	}
	if (!port) {
		port = UpnpGetServerPort();
	}

	SampleUtil_Print("UPnP Initialized\n"
			 "\tipaddress = %s port = %u\n",
			 ip_address ? ip_address : "{NULL}", port);
	SampleUtil_Print("Registering Control Point\n");


// write server system ipaddr & port
	if(fp)
	{
		fwrite("ipaddr", strlen("ipaddr"), 1, fp);
		fputc('\t', fp);
		fwrite(ip_address, strlen(ip_address), 1, fp);
		fputc('\n', fp);
		fwrite("port ", strlen("port"), 1, fp);
		fputc('\t', fp);
		fprintf(fp, "%d", port);
		fputc('\n', fp);
		fclose(fp);
	}

	rc = UpnpRegisterClient(TizenCtrlPointCallbackEventHandler,
				&ctrlpt_handle, &ctrlpt_handle);
	if (rc != UPNP_E_SUCCESS) {
		SampleUtil_Print("Error registering CP: %d\n", rc);
		UpnpFinish();

		return TIZEN_ERROR;
	}

	SampleUtil_Print("Control Point Registered\n");

	TizenCtrlPointRefresh();

	/* start a timer thread */
	ithread_create(&timer_thread, NULL, TizenCtrlPointTimerLoop, NULL);
	ithread_detach(timer_thread);

	return TIZEN_SUCCESS;
}

int TizenCtrlPointStop(void)
{
	TizenCtrlPointTimerLoopRun = 0;
	TizenCtrlPointRemoveAll();
	UpnpUnRegisterClient( ctrlpt_handle );
	UpnpFinish();
	SampleUtil_Finish();

	return TIZEN_SUCCESS;
}

void TizenCtrlPointPrintShortHelp(void)
{
	SampleUtil_Print(
		"Commands:\n"
		"  Help\n"
		"  HelpFull\n"
		"  ListDev\n"
		"  Refresh\n"
		"  PrintDev      <devnum>\n"
		"  PowerOn       <devnum>\n"
		"  PowerOff      <devnum>\n"
		"  SetChannel    <devnum> <channel>\n"
		"  SetVolume     <devnum> <volume>\n"
		"  SetColor      <devnum> <color>\n"
		"  SetTint       <devnum> <tint>\n"
		"  SetContrast   <devnum> <contrast>\n"
		"  SetBrightness <devnum> <brightness>\n"
		"  SendText      <devnum> <action>\n"
		"  CtrlAction    <devnum> <action>\n"
		"  PictAction    <devnum> <action>\n"
		"  CtrlGetVar    <devnum> <varname>\n"
		"  PictGetVar    <devnum> <action>\n"
		"  Exit\n");
}

void TizenCtrlPointPrintLongHelp(void)
{
	SampleUtil_Print(
		"\n"
		"******************************\n"
		"* TV Control Point Help Info *\n"
		"******************************\n"
		"\n"
		"This sample control point application automatically searches\n"
		"for and subscribes to the services of television device emulator\n"
		"devices, described in the tizendevicedesc.xml description document.\n"
		"It also registers itself as a tizen device.\n"
		"\n"
		"Commands:\n"
		"  Help\n"
		"       Print this help info.\n"
		"  ListDev\n"
		"       Print the current list of TV Device Emulators that this\n"
		"         control point is aware of.  Each device is preceded by a\n"
		"         device number which corresponds to the devnum argument of\n"
		"         commands listed below.\n"
		"  Refresh\n"
		"       Delete all of the devices from the device list and issue new\n"
		"         search request to rebuild the list from scratch.\n"
		"  PrintDev       <devnum>\n"
		"       Print the state table for the device <devnum>.\n"
		"         e.g., 'PrintDev 1' prints the state table for the first\n"
		"         device in the device list.\n"
		"  PowerOn        <devnum>\n"
		"       Sends the PowerOn action to the Control Service of\n"
		"         device <devnum>.\n"
		"  PowerOff       <devnum>\n"
		"       Sends the PowerOff action to the Control Service of\n"
		"         device <devnum>.\n"
		"  SetChannel     <devnum> <channel>\n"
		"       Sends the SetChannel action to the Control Service of\n"
		"         device <devnum>, requesting the channel to be changed\n"
		"         to <channel>.\n"
		"  SetVolume      <devnum> <volume>\n"
		"       Sends the SetVolume action to the Control Service of\n"
		"         device <devnum>, requesting the volume to be changed\n"
		"         to <volume>.\n"
		"  SetColor       <devnum> <color>\n"
		"       Sends the SetColor action to the Control Service of\n"
		"         device <devnum>, requesting the color to be changed\n"
		"         to <color>.\n"
		"  SetTint        <devnum> <tint>\n"
		"       Sends the SetTint action to the Control Service of\n"
		"         device <devnum>, requesting the tint to be changed\n"
		"         to <tint>.\n"
		"  SetContrast    <devnum> <contrast>\n"
		"       Sends the SetContrast action to the Control Service of\n"
		"         device <devnum>, requesting the contrast to be changed\n"
		"         to <contrast>.\n"
		"  SetBrightness  <devnum> <brightness>\n"
		"       Sends the SetBrightness action to the Control Service of\n"
		"         device <devnum>, requesting the brightness to be changed\n"
		"         to <brightness>.\n"
		"  CtrlAction     <devnum> <action>\n"
		"       Sends an action request specified by the string <action>\n"
		"         to the Control Service of device <devnum>.  This command\n"
		"         only works for actions that have no arguments.\n"
		"         (e.g., \"CtrlAction 1 IncreaseChannel\")\n"
		"  PictAction     <devnum> <action>\n"
		"       Sends an action request specified by the string <action>\n"
		"         to the Picture Service of device <devnum>.  This command\n"
		"         only works for actions that have no arguments.\n"
		"         (e.g., \"PictAction 1 DecreaseContrast\")\n"
		"  CtrlGetVar     <devnum> <varname>\n"
		"       Requests the value of a variable specified by the string <varname>\n"
		"         from the Control Service of device <devnum>.\n"
		"         (e.g., \"CtrlGetVar 1 Volume\")\n"
		"  PictGetVar     <devnum> <action>\n"
		"       Requests the value of a variable specified by the string <varname>\n"
		"         from the Picture Service of device <devnum>.\n"
		"         (e.g., \"PictGetVar 1 Tint\")\n"
		"  SendTxt     <devnum> <sting>\n"
		"       \n"
		"       \n"
		"       \n"
		"  Exit\n"
		"       Exits the control point application.\n");
}

/*! Tags for valid commands issued at the command prompt. */
enum cmdloop_tizencmds {
	PRTHELP = 0,
	PRTFULLHELP,
	POWON,
	POWOFF,
	SETCHAN,
	SETVOL,
	SETCOL,
	SETTINT,
	SETCONT,
	SETBRT,
	SENDTXT,
	CTRLACTION,
	PICTACTION,
	CTRLGETVAR,
	PICTGETVAR,
	PRTDEV,
	LSTDEV,
	REFRESH,
	EXITCMD
};

/*! Data structure for parsing commands from the command line. */
struct cmdloop_commands {
	/* the string  */
	const char *str;
	/* the command */
	int cmdnum;
	/* the number of arguments */
	int numargs;
	/* the args */
	const char *args;
} cmdloop_commands;

/*! Mappings between command text names, command tag,
 * and required command arguments for command line
 * commands */
static struct cmdloop_commands cmdloop_cmdlist[] = {
	{"Help",          PRTHELP,     1, ""},
	{"HelpFull",      PRTFULLHELP, 1, ""},
	{"ListDev",       LSTDEV,      1, ""},
	{"Refresh",       REFRESH,     1, ""},
	{"PrintDev",      PRTDEV,      2, "<devnum>"},
	{"PowerOn",       POWON,       2, "<devnum>"},
	{"PowerOff",      POWOFF,      2, "<devnum>"},
	{"SetChannel",    SETCHAN,     3, "<devnum> <channel (int)>"},
	{"SetVolume",     SETVOL,      3, "<devnum> <volume (int)>"},
	{"SetColor",      SETCOL,      3, "<devnum> <color (int)>"},
	{"SetTint",       SETTINT,     3, "<devnum> <tint (int)>"},
	{"SetContrast",   SETCONT,     3, "<devnum> <contrast (int)>"},
	{"SetBrightness", SETBRT,      3, "<devnum> <brightness (int)>"},
	{"SendText",      SENDTXT,     3, "<devnum> <string>"},
	{"CtrlAction",    CTRLACTION,  2, "<devnum> <action (string)>"},
	{"PictAction",    PICTACTION,  2, "<devnum> <action (string)>"},
	{"CtrlGetVar",    CTRLGETVAR,  2, "<devnum> <varname (string)>"},
	{"PictGetVar",    PICTGETVAR,  2, "<devnum> <varname (string)>"},
	{"Exit", EXITCMD, 1, ""}
};

void TizenCtrlPointPrintCommands(void)
{
	int i;
	int numofcmds = (sizeof cmdloop_cmdlist) / sizeof (cmdloop_commands);

	SampleUtil_Print("Valid Commands:\n");
	for (i = 0; i < numofcmds; ++i) {
		SampleUtil_Print("  %-14s %s\n",
			cmdloop_cmdlist[i].str, cmdloop_cmdlist[i].args);
	}
	SampleUtil_Print("\n");
}

void *TizenCtrlPointCommandLoop(void *args)
{
	char cmdline[100];
	int idx, cnt=0;
	int devnum = 0;
	struct TizenDeviceNode *tmpdevnode = NULL;
	
	
	int filereadlength=0;
	char filename[100] = {'\0'};
	
	while (1) {
		//		SampleUtil_Print("\n>> ");
		//		printf("[OCS] loop cnt : %d", cnt);
		//		fgets(cmdline, 100, stdin);
		//		TizenCtrlPointProcessCommand(cmdline);

		FILE * fp = fopen("filename.txt", "r");
		if(fp==NULL) 
		{
			printf("[OCS] File open fail!\n");
		}
		else
		{
			filereadlength = fread(filename, 50, 1, fp);
			filename[strlen(filename)-1] = '\0';
			fclose(fp);
		}
		printf("[OCS] filename : %s, cnt : %d\n", filename, cnt);

		devnum = 1;
		tmpdevnode = GlobalDeviceList;
		while(tmpdevnode) {
			TizenCtrlPointSendActionTextArg(devnum++, TIZEN_SERVICE_PICTURE, "SendText", "Text", filename);
			tmpdevnode = tmpdevnode->next;
		}

		sleep(1);
	}

	return NULL;
	args = args;
}

int TizenCtrlPointProcessCommand(char *cmdline)
{
	char cmd[100];
	char strarg[100];
	int arg_val_err = -99999;
	int arg1 = arg_val_err;
	int arg2 = arg_val_err;
	char arg3[100];//, validTextargs[100];
	int cmdnum = -1;
	int numofcmds = (sizeof cmdloop_cmdlist) / sizeof (cmdloop_commands);
	int cmdfound = 0;
	int i;
	int rc;
	int invalidargs = 0;
	int validargs, validTextargs;

	validargs = sscanf(cmdline, "%s %d %d", cmd, &arg1, &arg2);
	validTextargs = sscanf(cmdline, "%s %d %s", cmd, &arg1, arg3);
	for (i = 0; i < numofcmds; ++i) {
		if (strcasecmp(cmd, cmdloop_cmdlist[i].str ) == 0) {
			cmdnum = cmdloop_cmdlist[i].cmdnum;
			cmdfound++;
			if(strcasecmp(cmd, cmdloop_cmdlist[i].str ) == 0)
			{
				if (validTextargs != cmdloop_cmdlist[i].numargs)	{
					validTextargs++;
				}
			}
			else
			{
				if (validargs != cmdloop_cmdlist[i].numargs)	{
					invalidargs++;
				}
			}
			break;
		}
	}
	if (!cmdfound) {
		SampleUtil_Print("Command not found; try 'Help'\n");
		return TIZEN_SUCCESS;
	}
	if (invalidargs) {
		SampleUtil_Print("Invalid arguments; try 'Help'\n");
		return TIZEN_SUCCESS;
	}
	switch (cmdnum) {
	case PRTHELP:
		TizenCtrlPointPrintShortHelp();
		break;
	case PRTFULLHELP:
		TizenCtrlPointPrintLongHelp();
		break;
	case POWON:
		TizenCtrlPointSendPowerOn(arg1);
		break;
	case POWOFF:
		TizenCtrlPointSendPowerOff(arg1);
		break;
	case SETCHAN:
		TizenCtrlPointSendSetChannel(arg1, arg2);
		break;
	case SETVOL:
		TizenCtrlPointSendSetVolume(arg1, arg2);
		break;
	case SETCOL:
		TizenCtrlPointSendSetColor(arg1, arg2);
		break;
	case SETTINT:
		TizenCtrlPointSendSetTint(arg1, arg2);
		break;
	case SETCONT:
		TizenCtrlPointSendSetContrast(arg1, arg2);
		break;
	case SETBRT:
		TizenCtrlPointSendSetBrightness(arg1, arg2);
		break;
	case SENDTXT:
		TizenCtrlPointSendText(arg1, arg3);
		break;
	case CTRLACTION:
		/* re-parse commandline since second arg is string. */
		validargs = sscanf(cmdline, "%s %d %s", cmd, &arg1, strarg);
		if (validargs == 3)
			TizenCtrlPointSendAction(TIZEN_SERVICE_CONTROL, arg1, strarg,
				NULL, NULL, 0);
		else
			invalidargs++;
		break;
	case PICTACTION:
		/* re-parse commandline since second arg is string. */
		validargs = sscanf(cmdline, "%s %d %s", cmd, &arg1, strarg);
		if (validargs == 3)
			TizenCtrlPointSendAction(TIZEN_SERVICE_PICTURE, arg1, strarg,
				NULL, NULL, 0);
		else
			invalidargs++;
		break;
	case CTRLGETVAR:
		/* re-parse commandline since second arg is string. */
		validargs = sscanf(cmdline, "%s %d %s", cmd, &arg1, strarg);
		if (validargs == 3)
			TizenCtrlPointGetVar(TIZEN_SERVICE_CONTROL, arg1, strarg);
		else
			invalidargs++;
		break;
	case PICTGETVAR:
		/* re-parse commandline since second arg is string. */
		validargs = sscanf(cmdline, "%s %d %s", cmd, &arg1, strarg);
		if (validargs == 3)
			TizenCtrlPointGetVar(TIZEN_SERVICE_PICTURE, arg1, strarg);
		else
			invalidargs++;
		break;
	case PRTDEV:
		TizenCtrlPointPrintDevice(arg1);
		break;
	case LSTDEV:
		TizenCtrlPointPrintList();
		break;
	case REFRESH:
		TizenCtrlPointRefresh();
		break;
	case EXITCMD:
		rc = TizenCtrlPointStop();
		exit(rc);
		break;
	default:
		SampleUtil_Print("Command not implemented; see 'Help'\n");
		break;
	}
	if(invalidargs)
		SampleUtil_Print("Invalid args in command; see 'Help'\n");

	return TIZEN_SUCCESS;
}

/*! @} Control Point Sample Module */

/*! @} UpnpSamples */

