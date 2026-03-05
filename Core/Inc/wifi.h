/**
  ******************************************************************************
  * @file    wifi.h
  * @author  MCD Application Team
  * @brief   This file contains the different WiFi core resources definitions.
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
#ifndef WIFI_H
#define WIFI_H

#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "es_wifi.h"
#include "es_wifi_io.h"

/* Exported types ------------------------------------------------------------*/
typedef enum {
  WIFI_ECN_OPEN        = 0x00,
  WIFI_ECN_WEP         = 0x01,
  WIFI_ECN_WPA_PSK     = 0x02,
  WIFI_ECN_WPA2_PSK    = 0x03,
  WIFI_ECN_WPA_WPA2_PSK = 0x04
} WIFI_Ecn_t;

typedef enum {
  WIFI_TCP_PROTOCOL = 0,
  WIFI_UDP_PROTOCOL = 1,
} WIFI_Protocol_t;

typedef enum {
  WIFI_STATUS_OK          = 0,
  WIFI_STATUS_ERROR       = 1,
  WIFI_STATUS_NOT_SUPPORTED = 2,
  WIFI_STATUS_TIMEOUT     = 5
} WIFI_Status_t;

/* Exported functions ------------------------------------------------------- */
WIFI_Status_t WIFI_Init(void);
WIFI_Status_t WIFI_Connect(const char *SSID, const char *Password, WIFI_Ecn_t ecn);
WIFI_Status_t WIFI_Disconnect(void);
WIFI_Status_t WIFI_GetIP_Address(uint8_t *ipaddr);
WIFI_Status_t WIFI_GetHostAddress(const char *location, uint8_t *ipaddr);
WIFI_Status_t WIFI_OpenClientConnection(uint32_t socket, WIFI_Protocol_t type,
                                         const char *name, const uint8_t *ipaddr,
                                         uint16_t port, uint16_t local_port);
WIFI_Status_t WIFI_CloseClientConnection(uint32_t socket);
WIFI_Status_t WIFI_SendData(uint32_t socket, const uint8_t *pdata,
                             uint16_t Reqlen, uint16_t *SentDatalen,
                             uint32_t Timeout);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_H */
