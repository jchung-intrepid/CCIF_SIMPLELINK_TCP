/*
* SpyCCode.c - TCP socket sample application modified by Intrepid
*              Control Systems, Inc. for C Code Interface Project
*              by Vehicle Spy
*
* Copyright (C) 2014 Texas Instruments Incorporated - http://www.ti.com/
*
*
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions
*  are met:
*
*    Redistributions of source code must retain the above copyright
*    notice, this list of conditions and the following disclaimer.
*
*    Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in the
*    documentation and/or other materials provided with the
*    distribution.
*
*    Neither the name of Texas Instruments Incorporated nor the names of
*    its contributors may be used to endorse or promote products derived
*    from this software without specific prior written permission.
*
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
*  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
*  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
*  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
*  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
*  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
*  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
*  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
*  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
*  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
*  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
*/

/*
* Application Name     -	CCIF CC3100 TCP
* Application Overview -	This TCP client application opens socket
							connection to remote TCP server. 
							
							Data sent by remote server is received by
							BsdTcpClientReceiveProcess() on Spy_Main()
							which runs on a separate thread created by
							Vehicle Spy. Received data will be printed
							on CCIF's output console window as text.

							There are four Receive Message Event Handlers
							which fordwards the 8byte CAN message payload
							to TCP server when those messages are received
							by Vehicle Spy via the connected neoVI FIRE 
							adaptor. The application reads the value of the
							signal mapped to the 'Send CAND ata to TCP Server' 
							button in vspy's Graphical Panel window. This
							is used to enable / disable event handlers to
							send CAN data to the connected TCP server.
*/

////////////////////////////////////////
// Spy C Code Implementation
////////////////////////////////////////
#include "vspy.h"
#include "simplelink.h"
#include "sl_common.h"

// defines
#define CLI_Write				Printf

#define SL_STOP_TIMEOUT			0xFF

#define APPLICATION_VERSION		"1.0.0"

/* IP addressed of server side socket. Should be in long format,
* E.g: 0xc0a8010a == 192.168.1.10 */
#define IP_ADDR         0xc0a82b0f
#define PORT_NUM        5001 /* Port number to be used */

#define BUF_SIZE        1400
_u8 BsdBuf[BUF_SIZE];

_i16 ClientSockID = 0;

// forward protos
_i32 __cdecl configureSimpleLinkToDefaultState();
_i32 __cdecl establishConnectionWithAP();
_i32 __cdecl initializeAppVariables();
void __cdecl displayBanner();
_i32 __cdecl BsdTcpClientOpen(_u16 Port);
_i32 __cdecl BsdTcpClientClose();
_i32 __cdecl BsdTcpClientReceiveProcess();
_i32 __cdecl BsdTcpClientSendData();

// TODO: add global variables here
typedef enum _MAIN_STATE { /* States for the Spy_Main() loop state machine */
	eMAIN_STATE_IDLE,
	eMAIN_STATE_CC3100_CONNECT_TO_AP,
	eMAIN_STATE_CC3100_TCP_CLIENT_CONNECT,
	eMAIN_STATE_CC3100_TCP_CLIENT_PROCESS,
	eMAIN_STATE_CC3100_TCP_CLIENT_DISCONNECT

	// TODO: Design and add more states as needed

} MAIN_STATE;

MAIN_STATE main_state = eMAIN_STATE_IDLE;

typedef enum { /* Application specific status/error codes */
	DEVICE_NOT_IN_STATION_MODE = -0x7D0,        /* Choosing this number to avoid overlap w/ host-driver's error codes */
	TCP_SEND_ERROR = DEVICE_NOT_IN_STATION_MODE - 1,
	TCP_RECV_ERROR = TCP_SEND_ERROR - 1,

	STATUS_CODE_MAX = -0xBB8
}e_AppStatusCodes;

_u8 g_Status = 0;
_u8 is_cc3100_connected = 0;
_u8 is_socket_connected = 0;
_u8 send_can_msg_to_server_enabled = 0;

// TODO: add event handlers here
void SpyMsg_MG_neoECU_SIM_Msg_1_HS_CAN(MG_neoECU_SIM_Msg_1_HS_CAN * pMG_neoECU_SIM_Msg_1_HS_CAN)
{
	// Handler for knob1 Message
	if (send_can_msg_to_server_enabled && is_socket_connected)
	{
		/*unsigned char arbID[16];
		unsigned char buf[64];
		memset(arbID, 0, sizeof(arbID));
		memset(buf, 0, sizeof(buf));
		strcpy(buf, "ARBID = 0x");
		itoa(pMG_neoECU_SIM_Msg_1_HS_CAN->MessageData.iID, arbID, 16);
		strcat(buf, arbID);
		BsdTcpClientSendData(buf, strlen(buf));*/
		BsdTcpClientSendData(pMG_neoECU_SIM_Msg_1_HS_CAN->MessageData.btData, 
			pMG_neoECU_SIM_Msg_1_HS_CAN->MessageData.iNumDataBytes);
	}
}

void SpyMsg_MG_neoECU_SIM_Msg_2_HS_CAN(MG_neoECU_SIM_Msg_2_HS_CAN * pMG_neoECU_SIM_Msg_2_HS_CAN)
{
	// Uncomment to enable Knob2 Message
	return;
	// TODO: Add Event Code
	if (send_can_msg_to_server_enabled && is_socket_connected)
	{
		/* Press button from vspy's Graphical Panel to enable this CAN payload to be sent
		to connected TCP server*/
		BsdTcpClientSendData(pMG_neoECU_SIM_Msg_2_HS_CAN->MessageData.btData,
			pMG_neoECU_SIM_Msg_2_HS_CAN->MessageData.iNumDataBytes);
	}
}

void SpyMsg_MG_neoECU_SIM_Msg_3_HS_CAN(MG_neoECU_SIM_Msg_3_HS_CAN * pMG_neoECU_SIM_Msg_3_HS_CAN)
{
	// Uncomment to enable Knob3 Message
	return;
	// TODO: Add Event Code
	if (send_can_msg_to_server_enabled && is_socket_connected)
	{
		/* Press button from vspy's Graphical Panel to enable this CAN payload to be sent
		to connected TCP server*/
		BsdTcpClientSendData(pMG_neoECU_SIM_Msg_3_HS_CAN->MessageData.btData,
			pMG_neoECU_SIM_Msg_3_HS_CAN->MessageData.iNumDataBytes);
	}
}

void SpyMsg_MG_neoECU_SIM_Msg_4_HS_CAN(MG_neoECU_SIM_Msg_4_HS_CAN * pMG_neoECU_SIM_Msg_4_HS_CAN)
{
	// Uncomment to enable Knob4 Message
	return;
	// TODO: Add Event Code
	if (send_can_msg_to_server_enabled && is_socket_connected)
	{
		/* Press button from vspy's Graphical Panel to enable this CAN payload to be sent
		to connected TCP server*/
		BsdTcpClientSendData(pMG_neoECU_SIM_Msg_4_HS_CAN->MessageData.btData,
			pMG_neoECU_SIM_Msg_4_HS_CAN->MessageData.iNumDataBytes);
	}
}

void SpyAppSig_AS_send_can_msg_to_server(double dNewValue)
{
	// TODO: Add Event Code
	/* This function is called by vspy when you press the 'Send CAN Data to TCP Server'
	button from Graphical Panel (dNewValue = 1 when button is pressed 0 otherwise)*/
	send_can_msg_to_server_enabled = (unsigned int)dNewValue;
}

void Spy_EveryMessage(GenericMessage * p_Msg)
{
	// TODO: add something you want to do for every message
}

void Spy_EveryLongMessage(GenericLongMessage * p_Msg)
{
	// TODO: add something you want to do for every long message
}

void Spy_EveryLoop(unsigned int uiCurrentTime)
{
	// TODO: add something you want to do every millisecond
}

void Spy_EveryGUILoop()
{
	// TODO: write code to interact with the vehicle spy gui (this is called on the GUI thread)
}

void Spy_ErrorState(int iNetwork, int iTxErrorCount, int iRxErrorCount, int iErrorBitfield)
{
}

void Spy_ErrorFrame(int iNetwork, int iTxErrorCount, int iRxErrorCount, int iErrorBitfield)
{
}

void Spy_Stopped()
{
	// TODO: add stopped code
	_i32 retVal = -1;

	if (is_cc3100_connected)
	{
		retVal = sl_Stop(SL_STOP_TIMEOUT);
		if (retVal < 0)
			LOOP_FOREVER();
	}
}

void Spy_KeyPress(int iKey, int iCTRLPressed, int iALTPressed)
{
	// TODO: add key handler code
}

void Spy_Started()
{
	// TODO: add started code
}

void Spy_BeforeStarted()
{
	// TODO: add before started code
	_i32 retVal = -1;

	retVal = initializeAppVariables();

	displayBanner();

	/*
	* Following function configures the device to default state by cleaning
	* the persistent settings stored in NVMEM (viz. connection profiles &
	* policies, power policy etc)
	*
	* Applications may choose to skip this step if the developer is sure
	* that the device is in its default state at start of application
	*
	* Note that all profiles and persistent settings that were done on the
	* device will be lost
	*/
	retVal = configureSimpleLinkToDefaultState();
	if (retVal < 0)
	{
		CLI_Write(" Failed to configure the device in its default state \n\r");

		CLI_Write(" Is CC3100 Boosterpack powered? \n\r");

		main_state = eMAIN_STATE_IDLE;

		LOOP_FOREVER();

		return;
	}

	CLI_Write(" Device is configured in default state \n\r");

	/*
	* Assumption is that the device is configured in station mode already
	* and it is in its default state
	*/
	/* Initializing the CC3100 device */
	retVal = sl_Start(0, 0, 0);
	if ((retVal < 0) ||
		(ROLE_STA != retVal))
	{
		CLI_Write(" Failed to start the device \n\r");

		main_state = eMAIN_STATE_IDLE;

		LOOP_FOREVER();

		return;
	}

	CLI_Write(" Device started as STATION \n\r");

	is_cc3100_connected = 1;

	main_state = eMAIN_STATE_CC3100_CONNECT_TO_AP;
}

/*
* This is run on a separate thread by Vehicle Spy exe
*/
void Spy_Main()
{
	// TODO: Add code here to run every time Spy is run
	do 
	{
		switch (main_state)
		{
		case eMAIN_STATE_IDLE:
			// delay for one second
			Sleep(1000);
			// write to the output window
			Printf("Another Second happened\n");
			break;
		
		case eMAIN_STATE_CC3100_CONNECT_TO_AP:
			/* Connecting to WLAN AP - Set with static parameters defined in sl_common.h
			After this call we will be connected and have IP address */
			while (establishConnectionWithAP() != SUCCESS)
				Sleep(1000); // try again after 1 sec
			main_state = eMAIN_STATE_CC3100_TCP_CLIENT_CONNECT;
			break;

		case eMAIN_STATE_CC3100_TCP_CLIENT_CONNECT:
			while (BsdTcpClientOpen(PORT_NUM) != SUCCESS)
				Sleep(1000); // try again after 1 sec
			is_socket_connected = 1;
			/* let vspy know that socket is connected. This will cause 'TCP Connection' LED
			in Graphical Panels to turn Green */
			AS_is_socket_connected_Set(1);
			main_state = eMAIN_STATE_CC3100_TCP_CLIENT_PROCESS;
			break;

		case eMAIN_STATE_CC3100_TCP_CLIENT_PROCESS:
			/* Receive some data from server, echo back, and send received data as CAN
			message via HSCAN1 network of connected neoVI FIRE*/
			if (BsdTcpClientReceiveProcess() != SUCCESS)
				main_state = eMAIN_STATE_CC3100_TCP_CLIENT_DISCONNECT;
			break;

		case eMAIN_STATE_CC3100_TCP_CLIENT_DISCONNECT:
			/* Socket might be disconnected already but do this anyways to cleanup */
			BsdTcpClientClose();
			is_socket_connected = 0;
			/* let vspy know that socket is disconnected. This will cause 'TCP Connection' LED
			in Graphical Panels to turn Red */
			AS_is_socket_connected_Set(0);
			main_state = eMAIN_STATE_CC3100_CONNECT_TO_AP;
			break;

		// TODO: Design and add more states as needed

		}
	} while (1);
}

/*!
\brief Create a client TCP socket and attempt to connect to a TCP server

\param[in]	Port			TCP server socket port to connect to

\return                     On success, zero is returned.
On error, a negative number is returned.
*/
_i32 __cdecl BsdTcpClientOpen(_u16 Port)
{
	SlSockAddrIn_t  Addr;

	_u16          idx = 0;
	_u16          AddrSize = 0;
	_i16          Status = 0;
	_u16          LoopCount = 0;

	for (idx = 0; idx<BUF_SIZE; idx++)
	{
		BsdBuf[idx] = (_u8)(idx % 10);
	}

	Addr.sin_family = SL_AF_INET;
	Addr.sin_port = sl_Htons((_u16)Port);
	Addr.sin_addr.s_addr = sl_Htonl((_u32)IP_ADDR);
	AddrSize = sizeof(SlSockAddrIn_t);

	ClientSockID = sl_Socket(SL_AF_INET, SL_SOCK_STREAM, 0);
	if (ClientSockID < 0)
	{
		CLI_Write(" [TCP Client] Create socket Error \n\r");
		ASSERT_ON_ERROR(ClientSockID);
	}

	Status = sl_Connect(ClientSockID, (SlSockAddr_t *)&Addr, AddrSize);
	if (Status < 0)
	{
		sl_Close(ClientSockID);
		CLI_Write(" [TCP Client] TCP connection Error \n\r");
		ASSERT_ON_ERROR(Status);
	}

	return SUCCESS;
}

/*!
\brief receives at most BUF_SIZE bytes from another socket, then echoes it 
back to the sender.

\param[in]					None

\return                     On success, zero is returned.
On error, a negative number is returned.

*/
_i32 __cdecl BsdTcpClientReceiveProcess()
{
	_i32          Status = -1;
	unsigned int index = 0;

	// receive some data from server
	Status = sl_Recv(ClientSockID, &BsdBuf[0], BUF_SIZE, 0);
	if (Status <= 0)
	{
		sl_Close(ClientSockID);
		CLI_Write(" [TCP Client ] Data recv Error \n\r");
		ASSERT_ON_ERROR(TCP_RECV_ERROR);
	}

	CLI_Write("Received %d Bytes from TCP Client\n", Status);

	// echo back to server
	Status = sl_Send(ClientSockID, &BsdBuf[0], Status, 0);
	if (Status <= 0)
	{
		sl_Close(ClientSockID);
		CLI_Write(" [TCP Client] Data send Error \n\r");
		ASSERT_ON_ERROR(TCP_RECV_ERROR);
	}

	// print what we received from server
	for (index = 0; index < Status; index++)
		CLI_Write("Data = %c\n", BsdBuf[index]);

	return SUCCESS;
}

/*!
\brief gracefully close socket

This function causes the system to release resources allocated to a socket.  \n
In case of TCP, the connection is terminated.

\param[in]					None

\return                     On success, zero is returned.
							On error, a negative number is returned.

*/
_i32 __cdecl BsdTcpClientClose()
{
	sl_Close(ClientSockID);

	return SUCCESS;
}


/*!
\brief write data to TCP socket

This function is used to transmit a message to another socket.
Returns immediately after sending data to device.
In case of TCP failure an async event SL_SOCKET_TX_FAILED_EVENT is going to
be received.

\param[in] pData            Points to a buffer containing
							the message to be sent
\param[in] DataLen          message size in bytes. Range: 1-1460 bytes

\return                     Return the number of bytes transmitted,
							or -1 if an error occurred

*/
_i32 __cdecl BsdTcpClientSendData(unsigned char* pData, unsigned int DataLen)
{
	_i32          Status = -1;

	Status = sl_Send(ClientSockID, pData, DataLen, 0);
	if (Status <= 0)
	{
		CLI_Write(" [TCP Client] Data send Error \n\r");
		Status = sl_Close(ClientSockID);
		ASSERT_ON_ERROR(TCP_SEND_ERROR);
	}

	return Status;
}

/*!
\brief This function configure the SimpleLink device in its default state. It:
- Sets the mode to STATION
- Configures connection policy to Auto and AutoSmartConfig
- Deletes all the stored profiles
- Enables DHCP
- Disables Scan policy
- Sets Tx power to maximum
- Sets power policy to normal
- Unregisters mDNS services
- Remove all filters

\param[in]      none

\return         On success, zero is returned. On error, negative is returned
*/
_i32 __cdecl configureSimpleLinkToDefaultState()
{
	SlVersionFull   ver = { 0 };
	_WlanRxFilterOperationCommandBuff_t  RxFilterIdMask = { 0 };

	_u8           val = 1;
	_u8           configOpt = 0;
	_u8           configLen = 0;
	_u8           power = 0;

	_i32          retVal = -1;
	_i32          mode = -1;
	
	mode = sl_Start(0, 0, 0);
	ASSERT_ON_ERROR(mode);

	/* If the device is not in station-mode, try configuring it in staion-mode */
	if (ROLE_STA != mode)
	{
		if (ROLE_AP == mode)
		{
			/* If the device is in AP mode, we need to wait for this event before doing anything */
			while (!IS_IP_ACQUIRED(g_Status)) { Sleep(100); }
		}

		/* Switch to STA role and restart */
		retVal = sl_WlanSetMode(ROLE_STA);
		ASSERT_ON_ERROR(retVal);

		retVal = sl_Stop(SL_STOP_TIMEOUT);
		ASSERT_ON_ERROR(retVal);

		retVal = sl_Start(0, 0, 0);
		ASSERT_ON_ERROR(retVal);

		/* Check if the device is in station again */
		if (ROLE_STA != retVal)
		{
			/* We don't want to proceed if the device is not coming up in station-mode */
			ASSERT_ON_ERROR(DEVICE_NOT_IN_STATION_MODE);
		}
	}

	/* Get the device's version-information */
	configOpt = SL_DEVICE_GENERAL_VERSION;
	configLen = sizeof(ver);
	retVal = sl_DevGet(SL_DEVICE_GENERAL_CONFIGURATION, &configOpt, &configLen, (_u8 *)(&ver));
	ASSERT_ON_ERROR(retVal);

	/* Set connection policy to Auto + SmartConfig (Device's default connection policy) */
	retVal = sl_WlanPolicySet(SL_POLICY_CONNECTION, SL_CONNECTION_POLICY(1, 0, 0, 0, 1), NULL, 0);
	ASSERT_ON_ERROR(retVal);

	/* Remove all profiles */
	retVal = sl_WlanProfileDel(0xFF);
	ASSERT_ON_ERROR(retVal);

	/*
	* Device in station-mode. Disconnect previous connection if any
	* The function returns 0 if 'Disconnected done', negative number if already disconnected
	* Wait for 'disconnection' event if 0 is returned, Ignore other return-codes
	*/
	retVal = sl_WlanDisconnect();
	if (0 == retVal)
	{
		/* Wait */
		while (IS_CONNECTED(g_Status)) { Sleep(100); }
	}

	/* Enable DHCP client*/
	retVal = sl_NetCfgSet(SL_IPV4_STA_P2P_CL_DHCP_ENABLE, 1, 1, &val);
	ASSERT_ON_ERROR(retVal);

	/* Disable scan */
	configOpt = SL_SCAN_POLICY(0);
	retVal = sl_WlanPolicySet(SL_POLICY_SCAN, configOpt, NULL, 0);
	ASSERT_ON_ERROR(retVal);

	/* Set Tx power level for station mode
	Number between 0-15, as dB offset from max power - 0 will set maximum power */
	power = 0;
	retVal = sl_WlanSet(SL_WLAN_CFG_GENERAL_PARAM_ID, WLAN_GENERAL_PARAM_OPT_STA_TX_POWER, 1, (_u8 *)&power);
	ASSERT_ON_ERROR(retVal);

	/* Set PM policy to normal */
	retVal = sl_WlanPolicySet(SL_POLICY_PM, SL_NORMAL_POLICY, NULL, 0);
	ASSERT_ON_ERROR(retVal);

	/* Unregister mDNS services */
	retVal = sl_NetAppMDNSUnRegisterService(0, 0);
	ASSERT_ON_ERROR(retVal);

	/* Remove  all 64 filters (8*8) */
	pal_Memset(RxFilterIdMask.FilterIdMask, 0xFF, 8);
	retVal = sl_WlanRxFilterSet(SL_REMOVE_RX_FILTER, (_u8 *)&RxFilterIdMask,
		sizeof(_WlanRxFilterOperationCommandBuff_t));
	ASSERT_ON_ERROR(retVal);

	retVal = sl_Stop(SL_STOP_TIMEOUT);
	ASSERT_ON_ERROR(retVal);

	retVal = initializeAppVariables();
	ASSERT_ON_ERROR(retVal);

	return retVal; /* Success */
}

/*!
\brief Connecting to a WLAN Access point

This function connects to the required AP (SSID_NAME).
The function will return once we are connected and have acquired IP address

\param[in]  None

\return     0 on success, negative error-code on error

\note

\warning    If the WLAN connection fails or we don't acquire an IP address,
We will be stuck in this function forever.
*/
_i32 __cdecl establishConnectionWithAP()
{
	SlSecParams_t secParams = { 0 };
	_i32 retVal = 0;

	secParams.Key = PASSKEY;
	secParams.KeyLen = pal_Strlen(PASSKEY);
	secParams.Type = SEC_TYPE;

	retVal = sl_WlanConnect(SSID_NAME, pal_Strlen(SSID_NAME), 0, &secParams, 0);
	ASSERT_ON_ERROR(retVal);

	/* Wait */
	while ((!IS_CONNECTED(g_Status)) || (!IS_IP_ACQUIRED(g_Status))) { Sleep(100); }

	return SUCCESS;
}

/*!
\brief This function initializes the application variables

\param[in]  None

\return     0 on success, negative error-code on error
*/
_i32 __cdecl initializeAppVariables()
{
	g_Status = 0;
	is_cc3100_connected = 0;
	is_socket_connected = 0;
	send_can_msg_to_server_enabled = 0;
	main_state = eMAIN_STATE_IDLE;

	return SUCCESS;
}

/*!
\brief This function displays the application's banner

\param      None

\return     None
*/
void __cdecl displayBanner()
{
	CLI_Write("\n\r\n\r");
	CLI_Write(" My CCIF CC3100 Connect to AP Application - Version ");
	CLI_Write(APPLICATION_VERSION);
	CLI_Write("\n\r*******************************************************************************\n\r");
}

/*!
\brief This function handles WLAN events

\param[in]      pWlanEvent is the event passed to the handler

\return         None

\note

\warning
*/
void __cdecl SimpleLinkWlanEventHandler(SlWlanEvent_t *pWlanEvent)
{
	if (pWlanEvent == NULL)
	{
		CLI_Write(" [WLAN EVENT] NULL Pointer Error \n\r");
		return;
	}

	switch (pWlanEvent->Event)
	{
	case SL_WLAN_CONNECT_EVENT:
		{
			SET_STATUS_BIT(g_Status, STATUS_BIT_CONNECTION);

			/*
			* Information about the connected AP (like name, MAC etc) will be
			* available in 'slWlanConnectAsyncResponse_t' - Applications
			* can use it if required
			*
			* slWlanConnectAsyncResponse_t *pEventData = NULL;
			* pEventData = &pWlanEvent->EventData.STAandP2PModeWlanConnected;
			*
			*/
		}
		break;

	case SL_WLAN_DISCONNECT_EVENT:
		{
			slWlanConnectAsyncResponse_t*  pEventData = NULL;

			CLR_STATUS_BIT(g_Status, STATUS_BIT_CONNECTION);
			CLR_STATUS_BIT(g_Status, STATUS_BIT_IP_ACQUIRED);

			pEventData = &pWlanEvent->EventData.STAandP2PModeDisconnected;

			/* If the user has initiated 'Disconnect' request, 'reason_code' is
			* SL_USER_INITIATED_DISCONNECTION */
			if (SL_WLAN_DISCONNECT_USER_INITIATED_DISCONNECTION == pEventData->reason_code)
			{
				CLI_Write(" Device disconnected from the AP on application's request \n\r");
			}
			else
			{
				CLI_Write(" Device disconnected from the AP on an ERROR..!! \n\r");
			}
		}
		break;

	default:
		{
			CLI_Write(" [WLAN EVENT] Unexpected event \n\r");
		}
		break;
	}
}

/*!
\brief This function handles events for IP address acquisition via DHCP
indication

\param[in]      pNetAppEvent is the event passed to the handler

\return         None

\note

\warning
*/
void __cdecl SimpleLinkNetAppEventHandler(SlNetAppEvent_t *pNetAppEvent)
{
	if (pNetAppEvent == NULL)
	{
		CLI_Write(" [NETAPP EVENT] NULL Pointer Error \n\r");
		return;
	}

	switch (pNetAppEvent->Event)
	{
	case SL_NETAPP_IPV4_IPACQUIRED_EVENT:
		{
			SET_STATUS_BIT(g_Status, STATUS_BIT_IP_ACQUIRED);

			/*
			* Information about the connection (like IP, gateway address etc)
			* will be available in 'SlIpV4AcquiredAsync_t'
			* Applications can use it if required
			*
			* SlIpV4AcquiredAsync_t *pEventData = NULL;
			* pEventData = &pNetAppEvent->EventData.ipAcquiredV4;
			*
			*/
			int i = 0;
			unsigned char* ptrIP = (unsigned char*)&pNetAppEvent->EventData.ipAcquiredV4;
			CLI_Write(" [NETAPP EVENT] Acquired IP = ");
			for (i = 3; i >= 0; i--)
			{
				if (!i)
				{
					CLI_Write("%d", *ptrIP);
					break;
				}
				CLI_Write("%d.", *(ptrIP + i));
			}
			CLI_Write("\n");
		}
		break;

	default:
		{
			CLI_Write(" [NETAPP EVENT] Unexpected event \n\r");
		}
		break;
	}
}

/*!
\brief This function handles callback for the HTTP server events

\param[in]      pHttpEvent - Contains the relevant event information
\param[in]      pHttpResponse - Should be filled by the user with the
relevant response information

\return         None

\note

\warning
*/
void __cdecl SimpleLinkHttpServerCallback(SlHttpServerEvent_t *pHttpEvent,
	SlHttpServerResponse_t *pHttpResponse)
{
	CLI_Write(" [HTTP EVENT] \n\r");
}

/*!
\brief This function handles general error events indication

\param[in]      pDevEvent is the event passed to the handler

\return         None
*/
void __cdecl SimpleLinkGeneralEventHandler(SlDeviceEvent_t *pDevEvent)
{
	/*
	* Most of the general errors are not FATAL are are to be handled
	* appropriately by the application
	*/
	CLI_Write(" [GENERAL EVENT] \n\r");
}

/*!
\brief This function handles socket events indication

\param[in]      pSock is the event passed to the handler

\return         None
*/
void __cdecl SimpleLinkSockEventHandler(SlSockEvent_t *pSock)
{
	if (pSock == NULL)
	{
		CLI_Write(" [SOCK EVENT] NULL Pointer Error \n\r");
		return;
	}

	switch (pSock->Event)
	{
	case SL_SOCKET_TX_FAILED_EVENT:
		/*
		* TX Failed
		*
		* Information about the socket descriptor and status will be
		* available in 'SlSockEventData_t' - Applications can use it if
		* required
		*
		* SlSockEventData_u *pEventData = NULL;
		* pEventData = & pSock->socketAsyncEvent;
		*/
		switch (pSock->socketAsyncEvent.SockTxFailData.status)
		{
		case SL_ECLOSE:
			CLI_Write(" [SOCK EVENT] Close socket operation, failed to transmit all queued packets\n\r");
			break;
		default:
			CLI_Write(" [SOCK EVENT] Unexpected event \n\r");
			break;
		}
		break;

	default:
		CLI_Write(" [SOCK EVENT] Unexpected event \n\r");
		break;
	}
}
