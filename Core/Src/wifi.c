/**
  ******************************************************************************
  * @file    wifi.c
  * @author  MCD Application Team
  * @brief   WIFI interface file.
  *          Adapted to work with the es_wifi driver version in this project.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2017 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "wifi.h"
#include "es_wifi.h"
#include <string.h>

/* Private variables ---------------------------------------------------------*/
static ES_WIFIObject_t EsWifiObj;

/* Exported functions --------------------------------------------------------*/

WIFI_Status_t WIFI_Init(void)
{
  WIFI_Status_t ret = WIFI_STATUS_ERROR;

  if (ES_WIFI_RegisterBusIO(&EsWifiObj,
                             SPI_WIFI_Init,
                             SPI_WIFI_DeInit,
                             SPI_WIFI_Delay,
                             SPI_WIFI_SendData,
                             SPI_WIFI_ReceiveData) == ES_WIFI_STATUS_OK)
  {
    if (ES_WIFI_Init(&EsWifiObj) == ES_WIFI_STATUS_OK)
    {
      ret = WIFI_STATUS_OK;
    }
  }
  return ret;
}


WIFI_Status_t WIFI_Connect(const char *SSID, const char *Password, WIFI_Ecn_t ecn)
{
  WIFI_Status_t ret = WIFI_STATUS_ERROR;

  if (ES_WIFI_Connect(&EsWifiObj, SSID, Password,
                       (ES_WIFI_SecurityType_t)ecn) == ES_WIFI_STATUS_OK)
  {
    if (ES_WIFI_GetNetworkSettings(&EsWifiObj) == ES_WIFI_STATUS_OK)
    {
      ret = WIFI_STATUS_OK;
    }
  }
  return ret;
}


WIFI_Status_t WIFI_Disconnect(void)
{
  WIFI_Status_t ret = WIFI_STATUS_ERROR;

  if (ES_WIFI_Disconnect(&EsWifiObj) == ES_WIFI_STATUS_OK)
  {
    ret = WIFI_STATUS_OK;
  }
  return ret;
}


WIFI_Status_t WIFI_GetIP_Address(uint8_t *ipaddr)
{
  WIFI_Status_t ret = WIFI_STATUS_ERROR;

  if (ipaddr != NULL)
  {
    if (ES_WIFI_IsConnected(&EsWifiObj) == 1)
    {
      memcpy(ipaddr, EsWifiObj.NetSettings.IP_Addr, 4);
      ret = WIFI_STATUS_OK;
    }
  }
  return ret;
}


WIFI_Status_t WIFI_GetHostAddress(const char *location, uint8_t *ipaddr)
{
  WIFI_Status_t ret = WIFI_STATUS_ERROR;

  if (ipaddr != NULL)
  {
    if (ES_WIFI_DNS_LookUp(&EsWifiObj, location, ipaddr) == ES_WIFI_STATUS_OK)
    {
      ret = WIFI_STATUS_OK;
    }
  }
  return ret;
}


WIFI_Status_t WIFI_OpenClientConnection(uint32_t socket, WIFI_Protocol_t type,
                                         const char *name, const uint8_t *ipaddr,
                                         uint16_t port, uint16_t local_port)
{
  WIFI_Status_t ret = WIFI_STATUS_ERROR;
  ES_WIFI_Conn_t conn;

  conn.Number    = (uint8_t)socket;
  conn.RemotePort = port;
  conn.LocalPort  = local_port;
  conn.Type = (type == WIFI_TCP_PROTOCOL) ? ES_WIFI_TCP_CONNECTION : ES_WIFI_UDP_CONNECTION;
  conn.RemoteIP[0] = ipaddr[0];
  conn.RemoteIP[1] = ipaddr[1];
  conn.RemoteIP[2] = ipaddr[2];
  conn.RemoteIP[3] = ipaddr[3];

  if (ES_WIFI_StartClientConnection(&EsWifiObj, &conn) == ES_WIFI_STATUS_OK)
  {
    ret = WIFI_STATUS_OK;
  }
  return ret;
}


WIFI_Status_t WIFI_CloseClientConnection(uint32_t socket)
{
  WIFI_Status_t ret = WIFI_STATUS_ERROR;
  ES_WIFI_Conn_t conn;
  conn.Number = (uint8_t)socket;

  if (ES_WIFI_StopClientConnection(&EsWifiObj, &conn) == ES_WIFI_STATUS_OK)
  {
    ret = WIFI_STATUS_OK;
  }
  return ret;
}


WIFI_Status_t WIFI_SendData(uint32_t socket, const uint8_t *pdata,
                             uint16_t Reqlen, uint16_t *SentDatalen,
                             uint32_t Timeout)
{
  WIFI_Status_t ret = WIFI_STATUS_ERROR;

  if (ES_WIFI_SendData(&EsWifiObj, (uint8_t)socket,
                        (uint8_t *)pdata, Reqlen,
                        SentDatalen, Timeout) == ES_WIFI_STATUS_OK)
  {
    ret = WIFI_STATUS_OK;
  }
  return ret;
}
