/******************************************************************************
* File Name:   ota_context.h
*
* Description: Definitions and data structures for the OTA example application
*
* Related Document: See Readme.md
*
********************************************************************************
* Copyright 2021-2024, Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
*
* This software, including source code, documentation and related
* materials ("Software") is owned by Cypress Semiconductor Corporation
* or one of its affiliates ("Cypress") and is protected by and subject to
* worldwide patent protection (United States and foreign),
* United States copyright laws and international treaty provisions.
* Therefore, you may use this Software only as provided in the license
* agreement accompanying the software package from which you
* obtained this Software ("EULA").
* If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
* non-transferable license to copy, modify, and compile the Software
* source code solely for use in connection with Cypress's
* integrated circuit products.  Any reproduction, modification, translation,
* compilation, or representation of this Software except as specified
* above is prohibited without the express written permission of Cypress.
*
* Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
* reserves the right to make changes to the Software without notice. Cypress
* does not assume any liability arising out of the application or use of the
* Software or any product or circuit described in the Software. Cypress does
* not authorize its products for use in any products where a malfunction or
* failure of the Cypress product may reasonably be expected to result in
* significant property damage, injury or death ("High Risk Product"). By
* including Cypress's product in a High Risk Product, the manufacturer
* of such system or application assumes all risk of such use and in doing
* so agrees to indemnify Cypress against all liability.
*******************************************************************************/
#ifndef OTA_CONTEXT_H_
#define OTA_CONTEXT_H_

#include "cy_ota_api.h"
#include "wiced_bt_types.h"
#include "wiced_bt_dev.h"
#include "wiced_bt_gatt.h"
#include "wiced_bt_ble.h"

/******************************************************
 *                     Macros
 ******************************************************/

/******************************************************
 *                    Constants
 ******************************************************/

#define OTA_APP_TAG_VALID               (0x51EDBA15)
#define OTA_APP_TAG_INVALID             (0xDEADBEEF)


/******************************************************
 *            OTA example app type definitions
 ******************************************************/

typedef struct
{
    uint32_t tag;

    cy_ota_context_ptr      ota_context;
    cy_ota_connection_t     connection_type;

    /* Reboot when OTA is complete */
    uint8_t                 reboot_at_end;  /* 0 = do NOT reboot, 1 = reboot */

    uint16_t                    bt_conn_id;                 /* Host BT Connection ID */
    uint8_t                     bt_peer_addr[BD_ADDR_LEN];  /* Host BT address */
    wiced_bt_ble_conn_params_t  bt_conn_params;             /* BT connection parameters */
    uint16_t                    bt_ota_config_descriptor;       /* BT OTA configuration descriptor to determine if Device sends Notification/Indication */

} app_context_t;

/******************************************************
 *               Function Declarations
 ******************************************************/
void                   app_bt_initialize_default_values       (void);

#endif /* #define OTA_CONTEXT_H_ */
