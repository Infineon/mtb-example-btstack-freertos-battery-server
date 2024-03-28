/******************************************************************************
* File Name: main.c
*
* Description: This is the source code for the Bluetooth LE Battery Server
* Example for ModusToolbox. The Battery Service exposes the Battery Level
* of the device and comes with support for  OTA update over Bluetooth LE.
* A peer app on windows/Android/iOS can be used to push OTA update to the
* device. The app downloads and writes the image to the secondary slot.
* On the next reboot, MCUBoot copies the new image over to the primary slot
* and runs the application. If the new image is not validated in runtime, on
* the next reboot, MCUboot reverts to the previously validated image.
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

/*******************************************************************************
*        Header Files
*******************************************************************************/

/* Header file includes */
#include <string.h>
#include "cyhal.h"
#include "cybsp.h"
#include "cy_retarget_io.h"
#include "cybt_platform_trace.h"
#include "GeneratedSource/cycfg_gatt_db.h"
#include "GeneratedSource/cycfg_bt_settings.h"
#include "app_bt_utils.h"
#include "wiced_bt_ble.h"
#include "wiced_bt_uuid.h"
#include "wiced_memory.h"
#include "wiced_bt_stack.h"
#include "cycfg_bt_settings.h"
#include "cycfg_gap.h"
#include "cyhal_gpio.h"
#include "wiced_bt_l2c.h"
#include "cyabs_rtos.h"
#include "cy_log.h"
#include "stdlib.h"
#include "ota.h"
#include <inttypes.h>
/* FreeRTOS header file */
#include <FreeRTOS.h>
#include <task.h>

/* OTA related header files */
#include "cy_ota_api.h"
#include "ota_context.h"

#ifdef OTA_USE_EXTERNAL_FLASH
#include "ota_serial_flash.h"
#endif
#include "cyhal_wdt.h"

/*******************************************************************************
*        Macro Definitions
*******************************************************************************/
/**
 * @brief Typdef for function used to free allocated buffer to stack
 */
typedef void (*pfn_free_buffer_t)(uint8_t *);

/**
 * @brief rate of change of battery level
 */
#define BATTERY_LEVEL_CHANGE (2)

/**
 * @brief LED pin assignments for advertising event
 */
#define ADV_LED_GPIO CYBSP_USER_LED1

/**
 * @brief PWM frequency of LED's in Hz when blinking
 */
#define ADV_LED_PWM_FREQUENCY (1)

/**
 * @brief Update rate of Battery level
 */
#define BATTERY_LEVEL_UPDATE_MS   (9999u)
#define BATTERY_LEVEL_UPDATE_FREQ (10000)

/**
 * @brief PWM Duty Cycle of LED's for different states
 */
enum
{
    LED_ON_DUTY_CYCLE = 0,
    LED_BLINKING_DUTY_CYCLE = 50,
    LED_OFF_DUTY_CYCLE = 100
} led_duty_cycles;

/**
 * @brief This enumeration combines the advertising, connection states from two
 *        different callbacks to maintain the status in a single state variable
 */
typedef enum
{
    APP_BT_ADV_OFF_CONN_OFF,
    APP_BT_ADV_ON_CONN_OFF,
    APP_BT_ADV_OFF_CONN_ON
} app_bt_adv_conn_mode_t;

/*******************************************************************************
*        Variable Definitions
*******************************************************************************/
/**
 * @brief PWM Handle for controlling advertising LED
 */
static cyhal_pwm_t adv_led_pwm;

/**
 * @brief FreeRTOS variable to store handle of task created to update and send dummy
   values of temperature
 */
TaskHandle_t bas_task_handle;

/**
 * @brief variable to track connection and advertising state
 */
static app_bt_adv_conn_mode_t app_bt_adv_conn_state = APP_BT_ADV_OFF_CONN_OFF;

/**
 * @brief Variable for 5 sec timer object
 */
static cyhal_timer_t bas_timer_obj;

/**
 * @brief Configure timer for 5 sec
 */
const cyhal_timer_cfg_t bas_timer_cfg =
    {
        .compare_value = 0,                    /* Timer compare value, not used */
        .period = BATTERY_LEVEL_UPDATE_MS, /* Defines the timer period */
        .direction = CYHAL_TIMER_DIR_UP,       /* Timer counts up */
        .is_compare = false,                   /* Don't use compare mode */
        .is_continuous = true,                 /* Run timer indefinitely */
        .value = 0                             /* Initial value of counter */
};
/*******************************************************************************
*        Function Prototypes
*******************************************************************************/


/* GATT Event Callback Functions */

static wiced_bt_gatt_status_t app_bt_gatt_req_read_handler          (uint16_t conn_id,
                                                                     wiced_bt_gatt_opcode_t opcode,
                                                                     wiced_bt_gatt_read_t *p_read_req,
                                                                     uint16_t len_requested, 
                                                                     uint16_t *p_error_handle);
static wiced_bt_gatt_status_t app_bt_gatt_req_read_multi_handler    (uint16_t conn_id,
                                                                     wiced_bt_gatt_opcode_t opcode,
                                                                     wiced_bt_gatt_read_multiple_req_t *p_read_req,
                                                                     uint16_t len_requested, 
                                                                     uint16_t *p_error_handle);
static wiced_bt_gatt_status_t app_bt_gatt_req_read_by_type_handler  (uint16_t conn_id,
                                                                     wiced_bt_gatt_opcode_t opcode,
                                                                     wiced_bt_gatt_read_by_type_t *p_read_req,
                                                                     uint16_t len_requested, 
                                                                     uint16_t *p_error_handle);
static wiced_bt_gatt_status_t app_bt_connect_event_handler          (wiced_bt_gatt_connection_status_t *p_conn_status);
static wiced_bt_gatt_status_t app_bt_server_event_handler           (wiced_bt_gatt_event_data_t *p_data, 
                                                                     uint16_t *p_error_handle);
static wiced_bt_gatt_status_t app_bt_gatt_event_callback            (wiced_bt_gatt_evt_t event,
                                                                     wiced_bt_gatt_event_data_t *p_event_data);
static wiced_bt_gatt_status_t app_bt_set_value                      (uint16_t attr_handle, 
                                                                     uint8_t *p_val, 
                                                                     uint16_t len);
/* Callback function for Bluetooth stack management type events */
static wiced_bt_dev_status_t  app_bt_management_callback            (wiced_bt_management_evt_t event,
                                                                     wiced_bt_management_evt_data_t *p_event_data);
static wiced_bt_gatt_status_t app_bt_write_handler                  (wiced_bt_gatt_event_data_t *p_data, 
                                                                     uint16_t *p_error_handle);

static void                   app_bt_adv_led_update                 (void);
static void                   app_bt_init                           (void);
static void                   app_bt_batt_level_init                (void);

/* Task to send notifications with dummy battery values */
void bas_task(void *pvParam);
/* HAL timer callback registered when timer reaches terminal count */
void bas_timer_callb(void *callback_arg, cyhal_timer_event_t event);

/******************************************************************************
 *                          Function Definitions
 ******************************************************************************/

/**
 * Function Name:
 * app_bt_alloc_buffer
 *
 * Function Description:
 * @brief  This Function allocates the buffer of requested length
 *
 * @param len            Length of the buffer
 *
 * @return uint8_t*      pointer to allocated buffer
 */
static uint8_t *app_bt_alloc_buffer(uint16_t len)
{
    uint8_t *p = (uint8_t *)malloc(len);
    cy_log_msg(CYLF_DEF, CY_LOG_DEBUG, "%s() len %d alloc %p \r\n", __FUNCTION__,
               len, p);
    return p;
}

/**
 * Function Name:
 * app_bt_free_buffer
 *
 * Function Description:
 * @brief  This Function frees the buffer requested
 *
 * @param p_data         pointer to the buffer to be freed
 *
 * @return void
 */
static void app_bt_free_buffer(uint8_t *p_data)
{
    if (p_data != NULL)
    {
        cy_log_msg(CYLF_DEF, CY_LOG_DEBUG, "%s()        free:%p \r\n",
                   __FUNCTION__, p_data);
        free(p_data);
    }
}

/**
 * Function Name:
 * main
 *
 * Function Description :
 *  @brief Entry point to the application. Set device configuration and start BT
 *         stack initialization.  The actual application initialization will happen
 *         when stack reports that BT device is ready.
 */
int main()
{
    cy_rslt_t cy_result;
    wiced_result_t  result;
    cyhal_wdt_t wdt_obj;
    BaseType_t rtos_result;

    /* Initialize the board support package */
    cy_result = cybsp_init();
    if (CY_RSLT_SUCCESS != cy_result)
    {
        CY_ASSERT(0);
    }

    /* Enable global interrupts */
    __enable_irq();

    /* Initialize retarget-io to use the debug UART port */
    cy_retarget_io_init(CYBSP_DEBUG_UART_TX, CYBSP_DEBUG_UART_RX,
                        CY_RETARGET_IO_BAUDRATE);

    /* default for all logging to WARNING */
    cy_log_init(CY_LOG_INFO, NULL, NULL);

    /* default for OTA logging to NOTICE */
    cy_ota_set_log_level(CY_LOG_INFO);

    /*Initialize QuadSPI if using external flash*/
#if defined(OTA_USE_EXTERNAL_FLASH)
    /* We need to init from every ext flash write
     * See ota_serial_flash.h
     */
    if (CY_RSLT_SUCCESS != ota_smif_initialize())
    {
        cy_log_msg(CYLF_DEF, CY_LOG_ERR,"QSPI initialization FAILED!!\r\n");
        CY_ASSERT(0 == 1);
    }
#endif /* OTA_USE_EXTERNAL_FLASH */

#ifdef TEST_REVERT
    cy_log_msg(CYLF_DEF, CY_LOG_INFO,"======================TESTING REVERT==========================\r\n");
    cy_log_msg(CYLF_DEF, CY_LOG_INFO,"===============================================================\r\n");
    cy_log_msg(CYLF_DEF, CY_LOG_INFO,"===============================================================\r\n");
    cy_log_msg(CYLF_DEF, CY_LOG_INFO,"=========================== Rebooting !!!======================\r\n");
    cy_log_msg(CYLF_DEF, CY_LOG_INFO,"===============================================================\r\n");
    NVIC_SystemReset();
#else
    /* Validate the update so we do not revert on reboot */
    cy_ota_storage_validated();
#endif

    cy_log_msg(CYLF_DEF, CY_LOG_INFO,"==========BTStack FreeRTOS Example====================\r\n");
    cy_log_msg(CYLF_DEF, CY_LOG_INFO,"========Battery Server Application Start========\r\n");
    cy_log_msg(CYLF_DEF, CY_LOG_INFO,"================================================\n");
    cy_log_msg(CYLF_DEF, CY_LOG_INFO,"Application version: %d.%d.%d\n", APP_VERSION_MAJOR, APP_VERSION_MINOR, APP_VERSION_BUILD);
    cy_log_msg(CYLF_DEF, CY_LOG_INFO,"================================================\n\n");

    /* Initialising the HCI UART for Host contol */
    cybt_platform_config_init(&cybsp_bt_platform_cfg);

    /* set default values for battery server context */
    app_bt_initialize_default_values();

    /* Clear watchdog so it doesn't reboot on us */
    cyhal_wdt_init(&wdt_obj, cyhal_wdt_get_max_timeout_ms());
    cyhal_wdt_free(&wdt_obj);

    /* Register call back and configuration with stack */
    result = wiced_bt_stack_init(app_bt_management_callback, &wiced_bt_cfg_settings);

    /* Check if stack initialization was successful */
    if (WICED_BT_SUCCESS != result)
    {
        cy_log_msg(CYLF_DEF, CY_LOG_ERR, "Bluetooth Stack Initialization failed!! \r\n");
        CY_ASSERT(0);
    }

    /*Create battery service task*/
    rtos_result = xTaskCreate(bas_task, "BAS Task", (configMINIMAL_STACK_SIZE * 4),
                                NULL, (configMAX_PRIORITIES - 3), &bas_task_handle);
    if(pdPASS != rtos_result)
    {
        cy_log_msg(CYLF_DEF, CY_LOG_ERR,"BAS task creation failed\n");
    }

    /* Start the FreeRTOS scheduler */
    vTaskStartScheduler();

    /* Should never get here */
    CY_ASSERT(0);
}

/**
* Function Name: app_bt_management_callback
*
* Function Description:
* @brief
*  This is a Bluetooth stack event handler function to receive management events
*  from the Bluetooth stack and process as per the application.
*
* @param wiced_bt_management_evt_t       Bluetooth LE event code of one byte length
* @param wiced_bt_management_evt_data_t  Pointer to Bluetooth LE management event
*                                        structures
*
* @return wiced_result_t Error code from WICED_RESULT_LIST or BT_RESULT_LIST
*
*/

wiced_result_t app_bt_management_callback(wiced_bt_management_evt_t event,
                                          wiced_bt_management_evt_data_t *p_event_data)
{
    wiced_result_t result  = WICED_BT_ERROR;
    wiced_bt_device_address_t bda = {0};
    wiced_bt_ble_advert_mode_t *p_adv_mode = NULL;
    wiced_bt_dev_encryption_status_t *p_status = NULL;

    cy_log_msg(CYLF_DEF, CY_LOG_DEBUG, "\r\n%s() Event: (%d) %s\r\n",
               __func__, event, get_bt_event_name(event));

    switch (event)
    {
    case BTM_ENABLED_EVT:
        /* Bluetooth Controller and Host Stack Enabled */

        if (WICED_BT_SUCCESS == p_event_data->enabled.status)
        {
            /* Initialize the application */
            wiced_bt_set_local_bdaddr((uint8_t *)cy_bt_device_address, BLE_ADDR_PUBLIC);
            /* Bluetooth is enabled */
            wiced_bt_dev_read_local_addr(bda);
            cy_log_msg(CYLF_DEF, CY_LOG_NOTICE, "Local Bluetooth Address: ");
            print_bd_address(bda);

            /* Perform application-specific initialization */
            app_bt_init();
            result = WICED_BT_SUCCESS;
        }
        else
        {
            cy_log_msg(CYLF_DEF, CY_LOG_ERR, "Failed to initialize Bluetooth controller and stack \r\n");
        }

        break;

    case BTM_USER_CONFIRMATION_REQUEST_EVT:
        cy_log_msg(CYLF_DEF, CY_LOG_NOTICE,"* TM_USER_CONFIRMATION_REQUEST_EVT: Numeric_value = %"PRIu32" *\r",
                p_event_data->user_confirmation_request.numeric_value);
        wiced_bt_dev_confirm_req_reply(WICED_BT_SUCCESS, p_event_data->user_confirmation_request.bd_addr);
        result = WICED_BT_SUCCESS;
        break;

    case BTM_PASSKEY_NOTIFICATION_EVT:
         cy_log_msg(CYLF_DEF, CY_LOG_NOTICE,"PassKey Notification from BDA: ");
         cy_log_msg(CYLF_DEF, CY_LOG_NOTICE,"PassKey: %"PRIu32" \n", p_event_data->user_passkey_notification.passkey );
        result = WICED_BT_SUCCESS;
        break;

    case BTM_PAIRING_IO_CAPABILITIES_BLE_REQUEST_EVT:
        cy_log_msg(CYLF_DEF, CY_LOG_NOTICE, "  BTM_PAIRING_IO_CAPABILITIES_BLE_REQUEST_EVT\r\n");
        p_event_data->pairing_io_capabilities_ble_request.local_io_cap = BTM_IO_CAPABILITIES_NONE;
        p_event_data->pairing_io_capabilities_ble_request.oob_data = BTM_OOB_NONE;
        p_event_data->pairing_io_capabilities_ble_request.auth_req = BTM_LE_AUTH_REQ_BOND | BTM_LE_AUTH_REQ_MITM;
        p_event_data->pairing_io_capabilities_ble_request.max_key_size = 0x10;
        p_event_data->pairing_io_capabilities_ble_request.init_keys = BTM_LE_KEY_PENC | BTM_LE_KEY_PID;
        p_event_data->pairing_io_capabilities_ble_request.resp_keys = BTM_LE_KEY_PENC | BTM_LE_KEY_PID;
        result = WICED_BT_SUCCESS;
        break;

    case BTM_PAIRING_COMPLETE_EVT:
        cy_log_msg(CYLF_DEF, CY_LOG_NOTICE, "  Pairing Complete: %d ",
                   p_event_data->pairing_complete.pairing_complete_info.ble.reason);
        result = WICED_BT_SUCCESS;
        break;

    case BTM_LOCAL_IDENTITY_KEYS_UPDATE_EVT:
        /* Local identity Keys Update */
        result = WICED_BT_SUCCESS;
        break;

    case BTM_LOCAL_IDENTITY_KEYS_REQUEST_EVT:
        /* Local identity Keys Request */
        result = WICED_BT_ERROR;
        break;

    case BTM_PAIRED_DEVICE_LINK_KEYS_UPDATE_EVT:
        /* Paired Device Link Keys update */
        result = WICED_BT_SUCCESS;
        break;

    case BTM_PAIRED_DEVICE_LINK_KEYS_REQUEST_EVT:
        /* Paired Device Link Keys Request */
        result = WICED_BT_ERROR;
        break;


    case BTM_ENCRYPTION_STATUS_EVT:
        p_status = &p_event_data->encryption_status;
        cy_log_msg(CYLF_DEF, CY_LOG_NOTICE, "  Encryption Status Event for : bd ");
        print_bd_address(p_status->bd_addr);
        cy_log_msg(CYLF_DEF, CY_LOG_NOTICE, "  res: %d \r\n", p_status->result);
        result = WICED_BT_SUCCESS;
        break;

    case BTM_SECURITY_REQUEST_EVT:
        cy_log_msg(CYLF_DEF, CY_LOG_NOTICE, "  BTM_SECURITY_REQUEST_EVT\r\n");
        wiced_bt_ble_security_grant(p_event_data->security_request.bd_addr,
                                    WICED_BT_SUCCESS);
        result = WICED_BT_SUCCESS;
        break;

    case BTM_BLE_CONNECTION_PARAM_UPDATE:
        cy_log_msg(CYLF_DEF, CY_LOG_NOTICE, "BTM_BLE_CONNECTION_PARAM_UPDATE \r\n");
        cy_log_msg(CYLF_DEF, CY_LOG_NOTICE, "ble_connection_param_update.bd_addr: ");
        print_bd_address(p_event_data->ble_connection_param_update.bd_addr);
        cy_log_msg(CYLF_DEF, CY_LOG_NOTICE, "ble_connection_param_update.conn_interval       : %d\r\n",
                   p_event_data->ble_connection_param_update.conn_interval);
        cy_log_msg(CYLF_DEF, CY_LOG_NOTICE, "ble_connection_param_update.conn_latency        : %d\r\n",
                   p_event_data->ble_connection_param_update.conn_latency);
        cy_log_msg(CYLF_DEF, CY_LOG_NOTICE, "ble_connection_param_update.supervision_timeout : %d\r\n",
                  p_event_data->ble_connection_param_update.supervision_timeout);
        cy_log_msg(CYLF_DEF, CY_LOG_NOTICE, "ble_connection_param_update.status              : %d\r\n\n",
                   p_event_data->ble_connection_param_update.status);
        result = WICED_BT_SUCCESS;
        break;

    case BTM_BLE_ADVERT_STATE_CHANGED_EVT:

        /* Advertisement State Changed */
        p_adv_mode = &p_event_data->ble_advert_state_changed;
        cy_log_msg(CYLF_DEF, CY_LOG_NOTICE, "Advertisement State Change: %s\r\n",
                   get_bt_advert_mode_name(*p_adv_mode));

        if (BTM_BLE_ADVERT_OFF == *p_adv_mode)
        {
            /* Advertisement Stopped */
            cy_log_msg(CYLF_DEF, CY_LOG_NOTICE, "Advertisement stopped\r\n");

            /* Check connection status after advertisement stops */
            if (battery_server_context.bt_conn_id == 0)
            {
                app_bt_adv_conn_state = APP_BT_ADV_OFF_CONN_OFF;
            }
            else
            {
                app_bt_adv_conn_state = APP_BT_ADV_OFF_CONN_ON;
            }
        }
        else
        {
            /* Advertisement Started */
            cy_log_msg(CYLF_DEF, CY_LOG_NOTICE, "Advertisement started\r\n");
            app_bt_adv_conn_state = APP_BT_ADV_ON_CONN_OFF;
        }

        /* Update Advertisement LED to reflect the updated state */
        app_bt_adv_led_update();
        result = WICED_BT_SUCCESS;
        break;

    default:
        cy_log_msg(CYLF_DEF, CY_LOG_INFO, "Unhandled Bluetooth Management Event: 0x%x %s\r\n",
                   event, get_bt_event_name(event));
        break;
    }

    return result;
}

/**
 *  Function Name:
 *  app_bt_init
 *
 *  Function Description:
 *  @brief  This function handles application level initialization tasks and is
 *          called from the BT management callback once the Bluetooth LE stack enabled event
 *          (BTM_ENABLED_EVT) is triggered This function is executed in the BTM_ENABLED_EVT
 *           management callback.
 *
 *  @param void
 *
 *  @return wiced_result_t WICED_SUCCESS or WICED_failure
 */
static void app_bt_init(void)
{
    cy_rslt_t cy_result = CY_RSLT_SUCCESS;
    wiced_bt_gatt_status_t status = WICED_BT_GATT_ERROR;
    wiced_result_t result;

    /* Initialize the PWM used for Advertising LED */
    cy_result = cyhal_pwm_init(&adv_led_pwm, ADV_LED_GPIO, NULL);

    /* PWM init failed. Stop program execution */
    if (CY_RSLT_SUCCESS != cy_result)
    {
        cy_log_msg(CYLF_DEF, CY_LOG_ERR, "Advertisement LED PWM Initialization has failed! \r\n");
        CY_ASSERT(0);
    }

    /* Disable pairing for this application */
    wiced_bt_set_pairable_mode(WICED_TRUE, 0);

    /* Set Advertisement Data */
    wiced_bt_ble_set_raw_advertisement_data(CY_BT_ADV_PACKET_DATA_SIZE,
                                            cy_bt_adv_packet_data);

    /* Register with BT stack to receive GATT callback */
    status = wiced_bt_gatt_register(app_bt_gatt_event_callback);
    cy_log_msg(CYLF_DEF, CY_LOG_INFO, "GATT event Handler registration status: %s \r\n",
               get_bt_gatt_status_name(status));

    /* Initialize GATT Database */
    status = wiced_bt_gatt_db_init(gatt_database, gatt_database_len, NULL);
    cy_log_msg(CYLF_DEF, CY_LOG_INFO, "GATT database initialization status: %s \r\n",
               get_bt_gatt_status_name(status));

    /* Start Undirected Bluetooth LE Advertisements on device startup.
     * The corresponding parameters are contained in 'app_bt_cfg.c' */
    result = wiced_bt_start_advertisements(BTM_BLE_ADVERT_UNDIRECTED_HIGH, 0, NULL);
    if (WICED_BT_SUCCESS != result)
    {
        cy_log_msg(CYLF_DEF, CY_LOG_ERR, "Advertisement cannot start because of error: %d \r\n",
                   result);
        CY_ASSERT(0);
    }

    cy_log_msg(CYLF_DEF, CY_LOG_INFO,"***********************************************\r\n");
    cy_log_msg(CYLF_DEF, CY_LOG_INFO,"**Discover device with \"Battery Server\" name*\r\n");
    cy_log_msg(CYLF_DEF, CY_LOG_INFO,"***********************************************\r\n\n");

}
/*
 Function name:
 bas_timer_callb

 Function Description:
 @brief  This callback function is invoked on timeout of 1 second timer.

 @param  void*: unused
 @param cyhal_timer_event_t: unused

 @return void
 */
void bas_timer_callb(void *callback_arg, cyhal_timer_event_t event)
{
    BaseType_t xHigherPriorityTaskWoken;
    xHigherPriorityTaskWoken = pdFALSE;
    vTaskNotifyGiveFromISR(bas_task_handle, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/*
 Function name:
 bas_task

 Function Description:
 @brief  This task updates dummy battery value every time it is notified
         and sends a notification to the connected peer

 @param  void*: unused

 @return void
 */
void bas_task(void *pvParam)
{

    cy_rslt_t cy_result = CY_RSLT_SUCCESS;



    /* Initialize the HAL timer used to count seconds */
    cy_result = cyhal_timer_init(&bas_timer_obj, NC, NULL);
    if (CY_RSLT_SUCCESS != cy_result)
    {
        cy_log_msg(CYLF_DEF, CY_LOG_ERR,"BAS timer init failed !\n");
    }
    /* Configure the timer for 5 seconds */
    cyhal_timer_configure(&bas_timer_obj, &bas_timer_cfg);
    cy_result = cyhal_timer_set_frequency(&bas_timer_obj, BATTERY_LEVEL_UPDATE_FREQ);
    if (CY_RSLT_SUCCESS != cy_result)
    {
        cy_log_msg(CYLF_DEF, CY_LOG_ERR,"BAS timer set freq failed !\n");
    }
    /* Register for a callback whenever timer reaches terminal count */
    cyhal_timer_register_callback(&bas_timer_obj, bas_timer_callb, NULL);
    cyhal_timer_enable_event(&bas_timer_obj, CYHAL_TIMER_IRQ_TERMINAL_COUNT, 3, true);

    /* Start battery level timer */
    app_bt_batt_level_init();

    while(true)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        /* Battery level is read from gatt db and is reduced by 2 percent
        * by default and initialized again to 100 once it reaches 0*/
        if (0 == app_bas_battery_level[0])
        {
            app_bas_battery_level[0] = 100;
        }
        else
        {
            app_bas_battery_level[0] = app_bas_battery_level[0] - BATTERY_LEVEL_CHANGE;
        }

        if (battery_server_context.bt_conn_id)
        {
            if (app_bas_battery_level_client_char_config[0] & GATT_CLIENT_CONFIG_NOTIFICATION)
            {
                wiced_bt_gatt_server_send_notification(battery_server_context.bt_conn_id,
                                                    HDLC_BAS_BATTERY_LEVEL_VALUE,
                                                    app_bas_battery_level_len,
                                                    app_bas_battery_level,NULL);
                cy_log_msg(CYLF_DEF, CY_LOG_NOTICE,"================================================\r\n");
                cy_log_msg(CYLF_DEF, CY_LOG_NOTICE,"Sending Notification: Battery level: %u\r\n",
                        app_bas_battery_level[0]);
                cy_log_msg(CYLF_DEF, CY_LOG_NOTICE,"================================================\r\n");
            }
        }
    }
}
/**
 * Function Name:
 * app_bt_gatt_event_callback
 *
 * Function Description:
 * @brief  This Function handles the all the GATT events - GATT Event Handler
 *
 * @param event            Bluetooth LE GATT event type
 * @param p_event_data     Pointer to Bluetooth LE GATT event data
 *
 * @return wiced_bt_gatt_status_t  Bluetooth LE GATT status
 */
static wiced_bt_gatt_status_t app_bt_gatt_event_callback(wiced_bt_gatt_evt_t event,
                                                         wiced_bt_gatt_event_data_t *p_event_data)
{
    wiced_bt_gatt_status_t status = WICED_BT_GATT_SUCCESS;

    uint16_t error_handle = 0;

    wiced_bt_gatt_attribute_request_t *p_attr_req = &p_event_data->attribute_request;

    /* Call the appropriate callback function based on the GATT event type,
     * and pass the relevant event
     * parameters to the callback function */
    switch (event)
    {
    case GATT_CONNECTION_STATUS_EVT:
        status = app_bt_connect_event_handler (&p_event_data->connection_status);
        break;

    case GATT_ATTRIBUTE_REQUEST_EVT:
        status = app_bt_server_event_handler (p_event_data, 
                                              &error_handle);
        if(status != WICED_BT_GATT_SUCCESS)
        {
           wiced_bt_gatt_server_send_error_rsp(p_attr_req->conn_id, 
                                               p_attr_req->opcode, 
                                               error_handle, 
                                               status);
        }

        break;
        /* GATT buffer request, typically sized to max of bearer mtu - 1 */
    case GATT_GET_RESPONSE_BUFFER_EVT:
        cy_log_msg(CYLF_DEF, CY_LOG_DEBUG, "%s() GATT_GET_RESPONSE_BUFFER_EVT\r\n", __func__);
        p_event_data->buffer_request.buffer.p_app_rsp_buffer = app_bt_alloc_buffer(p_event_data->buffer_request.len_requested);
        p_event_data->buffer_request.buffer.p_app_ctxt = (void *)app_bt_free_buffer;
        status = WICED_BT_GATT_SUCCESS;
        break;
        /* GATT buffer transmitted event,  check \ref wiced_bt_gatt_buffer_transmitted_t*/
    case GATT_APP_BUFFER_TRANSMITTED_EVT:
        cy_log_msg(CYLF_DEF, CY_LOG_DEBUG, "\r\n\n%s() GATT_APP_BUFFER_TRANSMITTED_EVT.\r\n", __func__);
        {
            pfn_free_buffer_t pfn_free = (pfn_free_buffer_t)p_event_data->buffer_xmitted.p_app_ctxt;

            /* If the buffer is dynamic, the context will point to a function to free it. */
            if (pfn_free)
                pfn_free(p_event_data->buffer_xmitted.p_app_data);

            status = WICED_BT_GATT_SUCCESS;
        }
        break;

    default:
        cy_log_msg(CYLF_DEF, CY_LOG_INFO, " Unhandled GATT Event \r\n");
        status = WICED_BT_GATT_ERROR;
        break;
    }

    return status;
}


/**
 * Function Name
 * app_bt_connect_event_handler
 *
 * Function Description
 * @brief   This callback function handles connection status changes.
 *
 * @param p_conn_status    Pointer to data that has connection details
 *
 * @return wiced_bt_gatt_status_t See possible status codes in wiced_bt_gatt_status_e
 * in wiced_bt_gatt.h
 */

static wiced_bt_gatt_status_t app_bt_connect_event_handler (wiced_bt_gatt_connection_status_t *p_conn_status)
{
    wiced_bt_gatt_status_t status = WICED_BT_GATT_ERROR;
    wiced_result_t result;

    if (NULL != p_conn_status)
    {
        if (p_conn_status->connected)
        {
            /* Device has connected */
            cy_log_msg(CYLF_DEF, CY_LOG_NOTICE, "Connected : BDA ");
            print_bd_address(p_conn_status->bd_addr);
            cy_log_msg(CYLF_DEF, CY_LOG_NOTICE, "Connection ID '%d'\r\n", p_conn_status->conn_id);

            /* Store the connection ID and peer BD Address */
            battery_server_context.bt_conn_id = p_conn_status->conn_id;
            memcpy(battery_server_context.bt_peer_addr, p_conn_status->bd_addr, BD_ADDR_LEN);

            /* Update the adv/conn state */
            app_bt_adv_conn_state = APP_BT_ADV_OFF_CONN_ON;
            /* Save BT connection ID in application data structure */
            battery_server_context.bt_conn_id = p_conn_status->conn_id;
            /* Save BT peer ADDRESS in application data structure */
            memcpy(battery_server_context.bt_peer_addr, p_conn_status->bd_addr, BD_ADDR_LEN);
        }
        else
        {
            /* Device has disconnected */
            cy_log_msg(CYLF_DEF, CY_LOG_NOTICE, "Disconnected : BDA ");
            print_bd_address(p_conn_status->bd_addr);
            cy_log_msg(CYLF_DEF, CY_LOG_NOTICE, "Connection ID '%d', Reason '%s'\r\n", p_conn_status->conn_id,
                       get_bt_gatt_disconn_reason_name(p_conn_status->reason));

            /* Set the connection id to zero to indicate disconnected state */
            battery_server_context.bt_conn_id = 0;

            /* Restart the advertisements */
            result = wiced_bt_start_advertisements(BTM_BLE_ADVERT_UNDIRECTED_HIGH, 0, NULL);
            if (WICED_BT_SUCCESS != result)
            {
                cy_log_msg(CYLF_DEF, CY_LOG_ERR, "Advertisement cannot start because of error: %d \r\n",
                           result);
                CY_ASSERT(0);
            }

            /* Update the adv/conn state */
            app_bt_adv_conn_state = APP_BT_ADV_ON_CONN_OFF;
        }

        /* Update Advertisement LED to reflect the updated state */
        app_bt_adv_led_update();

        status = WICED_BT_GATT_SUCCESS;
    }

    return status;
}

/**
 * Function Name:
 * app_bt_server_event_handler
 *
 * Function Description:
 * @brief  The callback function is invoked when GATT_ATTRIBUTE_REQUEST_EVT occurs
 *         in GATT Event handler function. GATT Server Event Callback function.
 *
 * @param p_data   Pointer to Bluetooth LE GATT request data
 *
 * @return wiced_bt_gatt_status_t  Bluetooth LE GATT status
 */
static wiced_bt_gatt_status_t app_bt_server_event_handler (wiced_bt_gatt_event_data_t *p_data, 
                                                           uint16_t *p_error_handle)
{
    wiced_bt_gatt_status_t status = WICED_BT_GATT_SUCCESS;
    wiced_bt_gatt_attribute_request_t   *p_att_req = &p_data->attribute_request;

    switch (p_att_req->opcode)
    {
    /* Attribute read notification (attribute value internally read from GATT database) */
    case GATT_REQ_READ:
    case GATT_REQ_READ_BLOB:
        cy_log_msg(CYLF_DEF, CY_LOG_DEBUG, "  %s() GATTS_REQ_TYPE_READ\r\n", __func__);
        status = app_bt_gatt_req_read_handler(p_att_req->conn_id, p_att_req->opcode,
                                              &p_att_req->data.read_req,
                                              p_att_req->len_requested, 
                                              p_error_handle);
        break;

    case GATT_REQ_READ_BY_TYPE:
        status = app_bt_gatt_req_read_by_type_handler(p_att_req->conn_id, p_att_req->opcode,
                                                      &p_att_req->data.read_by_type,
                                                      p_att_req->len_requested, 
                                                      p_error_handle);
        break;

    case GATT_REQ_READ_MULTI:
    case GATT_REQ_READ_MULTI_VAR_LENGTH:
        status = app_bt_gatt_req_read_multi_handler(p_att_req->conn_id, p_att_req->opcode,
                                                    &p_att_req->data.read_multiple_req,
                                                    p_att_req->len_requested, 
                                                    p_error_handle);
        break;

    case GATT_REQ_WRITE:
    case GATT_CMD_WRITE:
    case GATT_CMD_SIGNED_WRITE:
        cy_log_msg(CYLF_DEF, CY_LOG_DEBUG, "  %s() GATTS_REQ_WRITE\r\n", __func__);
        status = app_bt_write_handler(p_data, p_error_handle);
        if ((p_att_req->opcode == GATT_REQ_WRITE) && (status == WICED_BT_GATT_SUCCESS))
        {
            wiced_bt_gatt_write_req_t *p_write_request = &p_att_req->data.write_req;
            wiced_bt_gatt_server_send_write_rsp(p_att_req->conn_id, 
                                                p_att_req->opcode,
                                                p_write_request->handle);
        }
        break;

    case GATT_REQ_PREPARE_WRITE:
        cy_log_msg(CYLF_DEF, CY_LOG_DEBUG, "  %s() GATT_REQ_PREPARE_WRITE\r\n", __func__);
        status = WICED_BT_GATT_SUCCESS;
        break;

    case GATT_REQ_EXECUTE_WRITE:
        cy_log_msg(CYLF_DEF, CY_LOG_DEBUG, "  %s() GATTS_REQ_TYPE_WRITE_EXEC\r\n",
                   __func__);
        wiced_bt_gatt_server_send_execute_write_rsp(p_att_req->conn_id, 
                                                    p_att_req->opcode);
        status = WICED_BT_GATT_SUCCESS;
        break;

    case GATT_REQ_MTU:
        /* Application calls wiced_bt_gatt_server_send_mtu_rsp() with the desired mtu */
        cy_log_msg(CYLF_DEF, CY_LOG_DEBUG, "  %s() GATTS_REQ_TYPE_MTU\r\n", __func__);
        status = wiced_bt_gatt_server_send_mtu_rsp(p_att_req->conn_id,
                                                   p_att_req->data.remote_mtu,
                                                   wiced_bt_cfg_settings.p_ble_cfg->ble_max_rx_pdu_size);
        cy_log_msg(CYLF_DEF, CY_LOG_NOTICE, "    Set MTU size to: %d  status: 0x%d\r\n",
                    p_att_req->data.remote_mtu, status);
        break;

    case GATT_HANDLE_VALUE_CONF: /* Value confirmation */
        cy_log_msg(CYLF_DEF, CY_LOG_DEBUG, "  %s() GATTS_REQ_TYPE_CONF\r\n",
                   __func__);
        cy_ota_agent_state_t ota_lib_state;
        cy_ota_get_state(battery_server_context.ota_context, &ota_lib_state);
        if ((ota_lib_state == CY_OTA_STATE_OTA_COMPLETE) && /* Check if we completed the download before rebooting */
            (battery_server_context.reboot_at_end != 0))
        {
            cy_log_msg(CYLF_DEF, CY_LOG_NOTICE, "%s()   RESETTING NOW !!!!\r\n",
                       __func__);
            cy_rtos_delay_milliseconds(1000);
            NVIC_SystemReset();
        }
        else
        {
            cy_ota_agent_stop(&battery_server_context.ota_context); /* Stop OTA */
        }
        status = WICED_BT_GATT_SUCCESS;
        break;

    case GATT_HANDLE_VALUE_NOTIF:
        cy_log_msg(CYLF_DEF, CY_LOG_NOTICE, "  %s() GATT_HANDLE_VALUE_NOTIF - Notification Sent\r\n", __func__);
        status = WICED_BT_GATT_SUCCESS;
        break;

    default:
        cy_log_msg(CYLF_DEF, CY_LOG_DEBUG, "  %s() Unhandled Event opcode: %d\r\n",
                   __func__, p_att_req->opcode);
        status = WICED_BT_GATT_ERROR;
        break;
    }
    return status;
}

/**
 * Function Name:
 * app_bt_write_handler
 *
 * Function Description:
 * @brief  The function is invoked when GATTS_REQ_TYPE_WRITE is received from the
 *         client device and is invoked GATT Server Event Callback function. This
 *         handles "Write Requests" received from Client device.
 *
 * @param p_write_req   Pointer to Bluetooth LE GATT write request
 *
 * @return wiced_bt_gatt_status_t  Bluetooth LE GATT status
 */
static wiced_bt_gatt_status_t app_bt_write_handler(wiced_bt_gatt_event_data_t *p_data, 
                                                   uint16_t *p_error_handle)
{
    wiced_bt_gatt_write_req_t *p_write_req = &p_data->attribute_request.data.write_req;;
    cy_rslt_t result;

    *p_error_handle = p_write_req->handle;

    CY_ASSERT(( NULL != p_data ) && (NULL != p_write_req));

    switch (p_write_req->handle)
    {
    case HDLD_OTA_FW_UPGRADE_SERVICE_OTA_UPGRADE_CONTROL_POINT_CLIENT_CHAR_CONFIG:
    case HDLC_OTA_FW_UPGRADE_SERVICE_OTA_UPGRADE_CONTROL_POINT_VALUE:
    case HDLC_OTA_FW_UPGRADE_SERVICE_OTA_UPGRADE_DATA_VALUE:
        /*Call OTA write handler to handle OTA related writes*/
        result = app_bt_ota_write_handler(p_data, p_error_handle);
        return (result == CY_RSLT_SUCCESS) ? WICED_BT_GATT_SUCCESS : WICED_BT_GATT_ERROR;

    default:
        /* Handle normal (non-OTA) indication confirmation requests here */
        /* Attempt to perform the Write Request */
        return app_bt_set_value(p_write_req->handle,
                                p_write_req->p_val,
                                p_write_req->val_len);
        break;
    }
}

/**
 * Function Name:
 * app_bt_set_value
 *
 * Function Description:
 * @brief  The function is invoked by app_bt_write_handler to set a value
 *         to GATT DB.
 *
 * @param attr_handle  GATT attribute handle
 * @param p_val        Pointer to Bluetooth LE GATT write request value
 * @param len          length of GATT write request
 *
 * @return wiced_bt_gatt_status_t  Bluetooth LE GATT status
 */
static wiced_bt_gatt_status_t app_bt_set_value(uint16_t attr_handle, 
                                               uint8_t *p_val,
                                               uint16_t len)
{
    wiced_bt_gatt_status_t status = WICED_BT_GATT_INVALID_HANDLE;

    cy_log_msg(CYLF_DEF, CY_LOG_NOTICE, "%s() handle : 0x%x (%d)\r\n", __func__,
               attr_handle, attr_handle);

    for (int i = 0; i < app_gatt_db_ext_attr_tbl_size; i++)
    {
        /* Check for a matching handle entry */
        if (app_gatt_db_ext_attr_tbl[i].handle == attr_handle)
        {
            /* Detected a matching handle in the external lookup table */
            if (app_gatt_db_ext_attr_tbl[i].max_len >= len)
            {
                /* Value fits within the supplied buffer; copy over the value */
                app_gatt_db_ext_attr_tbl[i].cur_len = len;
                memset(app_gatt_db_ext_attr_tbl[i].p_data, 0x00, app_gatt_db_ext_attr_tbl[i].max_len);
                memcpy(app_gatt_db_ext_attr_tbl[i].p_data, p_val, app_gatt_db_ext_attr_tbl[i].cur_len);

                if (memcmp(app_gatt_db_ext_attr_tbl[i].p_data, p_val, app_gatt_db_ext_attr_tbl[i].cur_len) == 0)
                {
                    status = WICED_BT_GATT_SUCCESS;
                }

                if(app_gatt_db_ext_attr_tbl[i].handle == HDLD_BAS_BATTERY_LEVEL_CLIENT_CHAR_CONFIG)
                {
                    if (GATT_CLIENT_CONFIG_NOTIFICATION == app_bas_battery_level_client_char_config[0])
                    {
                        cy_log_msg(CYLF_DEF, CY_LOG_NOTICE, "Battery Server Notifications Enabled \r\n");
                    }
                    else
                    {
                        cy_log_msg(CYLF_DEF, CY_LOG_NOTICE, "Battery Server Notifications Disabled \r\n");
                    }

                }
            }
            else
            {
                /* Value to write will not fit within the table */
                status = WICED_BT_GATT_INVALID_ATTR_LEN;
                cy_log_msg(CYLF_DEF, CY_LOG_ERR, "Invalid attribute length\r\n");
            }
            break;
        }
    }
    if (WICED_BT_GATT_SUCCESS != status)
    {
        cy_log_msg(CYLF_DEF, CY_LOG_ERR, "%s() FAILED %d \r\n", __func__, status);
    }
    return status;
}
/**
 * Function Name:
 * app_bt_batt_level_init
 *
 * Function Description :
 *  @brief This function Starts the timer for updating Battery Level
 *
 * @return void
 */
static void app_bt_batt_level_init(void)
{
    /* Start the timer */
    if (CY_RSLT_SUCCESS != cyhal_timer_start(&bas_timer_obj))
    {
        cy_log_msg(CYLF_DEF, CY_LOG_ERR,"BAS timer start failed !");
        CY_ASSERT(0);
    }
}

/**
 * Function Name:
 * app_bt_adv_led_update
 *
 * Function Description :
 *  @brief This function updates the advertising LED state based on Bluetooth LE advertising/
 *         connection state.
 *
 * @return void
 */
static void app_bt_adv_led_update(void)
{
    cy_rslt_t cy_result = CY_RSLT_SUCCESS;

    /* Stop the advertising led pwm */
    cy_result = cyhal_pwm_stop(&adv_led_pwm);
    if (CY_RSLT_SUCCESS != cy_result)
    {
        cy_log_msg(CYLF_DEF, CY_LOG_ERR, "Failed to stop PWM !!\r\n");
    }

    /* Update LED state based on Bluetooth LE advertising/connection state.
     * LED OFF for no advertisement/connection, LED blinking for advertisement
     * state, and LED ON for connected state  */
    switch (app_bt_adv_conn_state)
    {
    case APP_BT_ADV_OFF_CONN_OFF:
        cy_result = cyhal_pwm_set_duty_cycle(&adv_led_pwm, LED_OFF_DUTY_CYCLE,
                                             ADV_LED_PWM_FREQUENCY);
        break;

    case APP_BT_ADV_ON_CONN_OFF:
        cy_result = cyhal_pwm_set_duty_cycle(&adv_led_pwm, LED_BLINKING_DUTY_CYCLE,
                                             ADV_LED_PWM_FREQUENCY);
        break;

    case APP_BT_ADV_OFF_CONN_ON:
        cy_result = cyhal_pwm_set_duty_cycle(&adv_led_pwm, LED_ON_DUTY_CYCLE,
                                             ADV_LED_PWM_FREQUENCY);
        break;

    default:
        /* LED OFF for unexpected states */
        cy_result = cyhal_pwm_set_duty_cycle(&adv_led_pwm, LED_OFF_DUTY_CYCLE,
                                             ADV_LED_PWM_FREQUENCY);
        break;
    }
    /* Check if update to PWM parameters is successful*/
    if (CY_RSLT_SUCCESS != cy_result)
    {
        cy_log_msg(CYLF_DEF, CY_LOG_ERR, "Failed to set duty cycle parameters!!\r\n");
    }

    /* Start the advertising led pwm */
    cy_result = cyhal_pwm_start(&adv_led_pwm);

    /* Check if PWM started successfully */
    if (CY_RSLT_SUCCESS != cy_result)
    {
        cy_log_msg(CYLF_DEF, CY_LOG_ERR, "Failed to start PWM !!\r\n");
    }
}

/**
 * Function Name:
 * app_bt_find_by_handle
 *
 * Function Description:
 * @brief  Find attribute description by handle
 *
 * @param handle    handle to look up
 *
 * @return gatt_db_lookup_table_t   pointer containing handle data
 */
static gatt_db_lookup_table_t *app_bt_find_by_handle(uint16_t handle)
{
    int i;
    for (i = 0; i < app_gatt_db_ext_attr_tbl_size; i++)
    {
        if (app_gatt_db_ext_attr_tbl[i].handle == handle)
        {
            return (&app_gatt_db_ext_attr_tbl[i]);
        }
    }
    return NULL;
}

/**
 * Function Name:
 * app_bt_gatt_req_read_handler
 *
 * Function Description:
 * @brief  This Function Process read request from peer device
 *
 * @param conn_id       Connection ID
 * @param opcode        Bluetooth LE GATT request type opcode
 * @param p_read_req    Pointer to read request containing the handle to read
 * @param len_requested length of data requested
 *
 * @return wiced_bt_gatt_status_t  Bluetooth LE GATT status
 */
static wiced_bt_gatt_status_t app_bt_gatt_req_read_handler(uint16_t conn_id,
                                                           wiced_bt_gatt_opcode_t opcode,
                                                           wiced_bt_gatt_read_t *p_read_req,
                                                           uint16_t len_requested, 
                                                           uint16_t *p_error_handle)
{
    gatt_db_lookup_table_t *puAttribute;
    uint16_t attr_len_to_copy, to_send;
    uint8_t *from;

    *p_error_handle = p_read_req->handle;

    if ((puAttribute = app_bt_find_by_handle(p_read_req->handle)) == NULL)
    {
        cy_log_msg(CYLF_DEF, CY_LOG_ERR, "%s()  Attribute not found, Handle: 0x%04x\r\n",
                    __func__, p_read_req->handle);
        return WICED_BT_GATT_INVALID_HANDLE;
    }

    attr_len_to_copy = puAttribute->cur_len;
    cy_log_msg(CYLF_DEF, CY_LOG_NOTICE, "%s() conn_id: %d handle:0x%04x offset:%d len:%d\r\n", __func__,
               conn_id, p_read_req->handle, p_read_req->offset, attr_len_to_copy);

    if (p_read_req->offset >= puAttribute->cur_len)
    {
        cy_log_msg(CYLF_DEF, CY_LOG_ERR, "%s() offset:%d larger than attribute length:%d\r\n", __func__,
                   p_read_req->offset, puAttribute->cur_len);

        return WICED_BT_GATT_INVALID_OFFSET;
    }

    to_send = MIN(len_requested, attr_len_to_copy - p_read_req->offset);
    from = puAttribute->p_data + p_read_req->offset;
    return wiced_bt_gatt_server_send_read_handle_rsp(conn_id, opcode, to_send, from, NULL); /* No need for context, as buff not allocated */
}

/**
 * Function Name:
 * app_bt_gatt_req_read_by_type_handler
 *
 * Function Description:
 * @brief  Process read-by-type request from peer device
 *
 * @param conn_id       Connection ID
 * @param opcode        Bluetooth LE GATT request type opcode
 * @param p_read_req    Pointer to read request containing the handle to read
 * @param len_requested length of data requested
 *
 * @return wiced_bt_gatt_status_t  Bluetooth LE GATT status
 */
static wiced_bt_gatt_status_t app_bt_gatt_req_read_by_type_handler(uint16_t conn_id,
                                                                   wiced_bt_gatt_opcode_t opcode,
                                                                   wiced_bt_gatt_read_by_type_t *p_read_req,
                                                                   uint16_t len_requested, 
                                                                   uint16_t *p_error_handle)
{
    gatt_db_lookup_table_t *puAttribute;
    uint16_t last_handle = 0;
    uint16_t attr_handle = p_read_req->s_handle;
    uint8_t *p_rsp = app_bt_alloc_buffer(len_requested);
    uint8_t pair_len = 0;
    int used = 0;

    if (p_rsp == NULL)
    {
        cy_log_msg(CYLF_DEF, CY_LOG_ERR, "%s() No memory, len_requested: %d!!\r\n",
                   __func__, len_requested);

        return WICED_BT_GATT_INSUF_RESOURCE;
    }

    /* Read by type returns all attributes of the specified type, between the start and end handles */
    while (WICED_TRUE)
    {
        *p_error_handle = attr_handle;
        last_handle = attr_handle;
        attr_handle = wiced_bt_gatt_find_handle_by_type(attr_handle, 
                                                        p_read_req->e_handle,
                                                        &p_read_req->uuid);

        if (attr_handle == 0)
            break;

        if ((puAttribute = app_bt_find_by_handle(attr_handle)) == NULL)
        {
            cy_log_msg(CYLF_DEF, CY_LOG_ERR, "%s()  found type but no attribute for %d \r\n",
                       __func__, last_handle);
            app_bt_free_buffer(p_rsp);
            return WICED_BT_GATT_INVALID_HANDLE;
        }

        {
            int filled = wiced_bt_gatt_put_read_by_type_rsp_in_stream(p_rsp + used,
                                                                      len_requested - used,
                                                                      &pair_len,
                                                                      attr_handle,
                                                                      puAttribute->cur_len,
                                                                      puAttribute->p_data);
            if (filled == 0)
            {
                break;
            }
            used += filled;
        }

        /* Increment starting handle for next search to one past current */
        attr_handle++;
    }

    if (used == 0)
    {
        cy_log_msg(CYLF_DEF, CY_LOG_ERR, "%s()  attr not found  start_handle: 0x%04x  "
                   "end_handle: 0x%04x  Type: 0x%04x\r\n",
                   __func__, p_read_req->s_handle, p_read_req->e_handle,
                   p_read_req->uuid.uu.uuid16);

        app_bt_free_buffer(p_rsp);
        return WICED_BT_GATT_INVALID_HANDLE;
    }

    /* Send the response */
    wiced_bt_gatt_server_send_read_by_type_rsp(conn_id, opcode, pair_len, used,
                                               p_rsp, (void *)app_bt_free_buffer);

    return WICED_BT_GATT_SUCCESS;
}

/**
 * Function Name:
 * app_bt_gatt_req_read_multi_handler
 *
 * Function Description:
 * @brief  Process write read multi request from peer device
 *
 * @param conn_id       Connection ID
 * @param opcode        Bluetooth LE GATT request type opcode
 * @param p_read_req    Pointer to read request containing the handle to read
 * @param len_requested length of data requested
 *
 * @return wiced_bt_gatt_status_t  Bluetooth LE GATT status
 */
static wiced_bt_gatt_status_t app_bt_gatt_req_read_multi_handler(uint16_t conn_id,
                                                                 wiced_bt_gatt_opcode_t opcode,
                                                                 wiced_bt_gatt_read_multiple_req_t *p_read_req,
                                                                 uint16_t len_requested, 
                                                                 uint16_t *p_error_handle)
{
    gatt_db_lookup_table_t *puAttribute;
    uint8_t *p_rsp = app_bt_alloc_buffer(len_requested);
    int used = 0;
    int xx;
    uint16_t handle = wiced_bt_gatt_get_handle_from_stream(p_read_req->p_handle_stream, 0);
    *p_error_handle = handle;

    if (p_rsp == NULL)
    {
        cy_log_msg(CYLF_DEF, CY_LOG_ERR, "%s() No memory len_requested: %d!!\r\n",
                    __func__, len_requested);
        return WICED_BT_GATT_INVALID_HANDLE;
    }

    /* Read by type returns all attributes of the specified type, between the
     * start and end handles */
    for (xx = 0; xx < p_read_req->num_handles; xx++)
    {
        handle = wiced_bt_gatt_get_handle_from_stream(p_read_req->p_handle_stream, xx);
        *p_error_handle = handle;
        if ((puAttribute = app_bt_find_by_handle(handle)) == NULL)
        {
            cy_log_msg(CYLF_DEF, CY_LOG_ERR, "%s()  no handle 0x%04x\r\n",
                       __func__, handle);
            app_bt_free_buffer(p_rsp);
            return WICED_BT_GATT_ERR_UNLIKELY;
        }

        {
            int filled = wiced_bt_gatt_put_read_multi_rsp_in_stream(opcode, p_rsp + used,
                                                                    len_requested - used,
                                                                    puAttribute->handle,
                                                                    puAttribute->cur_len,
                                                                    puAttribute->p_data);
            if (!filled)
            {
                break;
            }
            used += filled;
        }
    }

    if (used == 0)
    {
        cy_log_msg(CYLF_DEF, CY_LOG_ERR, "%s() no attr found\r\n", __func__);

        return WICED_BT_GATT_INVALID_HANDLE;
    }

    /* Send the response */
    wiced_bt_gatt_server_send_read_multiple_rsp(conn_id, opcode, used, p_rsp,
                                                (void *)app_bt_free_buffer);

    return WICED_BT_GATT_SUCCESS;
}
/* [] END OF FILE */
