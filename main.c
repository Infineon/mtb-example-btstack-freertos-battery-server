/******************************************************************************
* File Name: main.c
*
* Description: This is the source code for the AnyCloud: BLE Battery Server
*              Example for ModusToolbox.
*
* Related Document: See README.md
*
*******************************************************************************
* (c) 2020, Cypress Semiconductor Corporation. All rights reserved.
*******************************************************************************
* This software, including source code, documentation and related materials
* ("Software"), is owned by Cypress Semiconductor Corporation or one of its
* subsidiaries ("Cypress") and is protected by and subject to worldwide patent
* protection (United States and foreign), United States copyright laws and
* international treaty provisions. Therefore, you may use this Software only
* as provided in the license agreement accompanying the software package from
* which you obtained this Software ("EULA").
*
* If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
* non-transferable license to copy, modify, and compile the Software source
* code solely for use in connection with Cypress's integrated circuit products.
* Any reproduction, modification, translation, compilation, or representation
* of this Software except as specified above is prohibited without the express
* written permission of Cypress.
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
* including Cypress's product in a High Risk Product, the manufacturer of such
* system or application assumes all risk of such use and in doing so agrees to
* indemnify Cypress against all liability.
*******************************************************************************/

/*******************************************************************************
*        Header Files
*******************************************************************************/
#include "app_bt_cfg.h"
#include "wiced_bt_stack.h"
#include "cybsp.h"
#include "cy_retarget_io.h"
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <string.h>
#include "cybt_platform_trace.h"
#include "wiced_memory.h"
#include "cyhal.h"
#include "stdio.h"
#include "cycfg_gatt_db.h"
#include "wiced_bt_dev.h"
#include "app_bt_utils.h"
#include "cyabs_rtos.h"

/*******************************************************************************
*        Macro Definitions
*******************************************************************************/
/* LED pin assignments for advertising event */
#define ADV_LED_GPIO                    CYBSP_USER_LED1

/* PWM frequency of LED's in Hz when blinking */
#define ADV_LED_PWM_FREQUENCY           (1)

/* Update rate of Battery level*/
#define BATTERY_LEVEL_UPDATE_MS         (1000)

/* PWM Duty Cycle of LED's for different states */
enum
{
    LED_ON_DUTY_CYCLE = 0,
    LED_BLINKING_DUTY_CYCLE= 50,
    LED_OFF_DUTY_CYCLE = 100
} led_duty_cycles;

/* This enumeration combines the advertising, connection states from two different
 * callbacks to maintain the status in a single state variable */
typedef enum
{
    APP_BT_ADV_OFF_CONN_OFF,
    APP_BT_ADV_ON_CONN_OFF,
    APP_BT_ADV_OFF_CONN_ON
} app_bt_adv_conn_mode_t;

/******************************************************************************
 * For application trace printing, you could use either OS/platform-specific
 * trace function (for example, printf(...)) directly,
 * or the macro APP_TRACE_DEBUG which is the common logging interface defined
 * by the header file cybt_platform_trace.h.
 *
 * If APP_TRACE_DEBUG is selected, please check cybt_platform_trace.h and see
 * whether CYBT_PLATFORM_TRACE_ENABLE is defined and the default level value
 * INITIAL_TRACE_LEVEL_APP.
 *
 * BTW, WICED_BT_TRACE was used in WICED SDK, now you could re-define it
 * or replace it with above.
 ******************************************************************************/
#define WICED_BT_TRACE      APP_TRACE_DEBUG

/*******************************************************************************
*        Variable Definitions
*******************************************************************************/
static cyhal_pwm_t               adv_led_pwm;
static cy_timer_t                batt_level_timer;
static uint16_t                  bt_connection_id = 0;
static app_bt_adv_conn_mode_t    app_bt_adv_conn_state = APP_BT_ADV_OFF_CONN_OFF;

/*******************************************************************************
*        Function Prototypes
*******************************************************************************/
static void                   batt_level_timer_cb            (cy_timer_callback_arg_t cb_params);
static void                   adv_led_update                 (void);
static void                   ble_app_init                   (void);
static void                   ble_app_set_advertisement_data (void);
static void                   batt_level_init                (void);

/* GATT Event Callback Functions */
static wiced_bt_gatt_status_t ble_app_write_handler          (wiced_bt_gatt_write_t *p_write_req, uint16_t conn_id);
static wiced_bt_gatt_status_t ble_app_read_handler           (wiced_bt_gatt_read_t *p_read_req, uint16_t conn_id);
static wiced_bt_gatt_status_t ble_app_connect_callback       (wiced_bt_gatt_connection_status_t *p_conn_status);
static wiced_bt_gatt_status_t ble_app_server_callback        (uint16_t conn_id, wiced_bt_gatt_request_type_t type, wiced_bt_gatt_request_data_t *p_data);
static wiced_bt_gatt_status_t ble_app_gatt_event_handler     (wiced_bt_gatt_evt_t  event, wiced_bt_gatt_event_data_t *p_event_data);

/* Callback function for Bluetooth stack management type events */
static wiced_bt_dev_status_t  app_bt_management_callback     (wiced_bt_management_evt_t event, wiced_bt_management_evt_data_t *p_event_data);

/******************************************************************************
 *                          Function Definitions
 ******************************************************************************/
/*
 *  Entry point to the application. Set device configuration and start BT
 *  stack initialization.  The actual application initialization will happen
 *  when stack reports that BT device is ready.
 */
int main()
{
    cy_rslt_t result ;

    /* Initialize the board support package */
    result = cybsp_init();
    if (CY_RSLT_SUCCESS != result)
    {
        CY_ASSERT(0);
    }

    /* Enable global interrupts */
    __enable_irq();

    /* Initialize retarget-io to use the debug UART port */
    cy_retarget_io_init(CYBSP_DEBUG_UART_TX, CYBSP_DEBUG_UART_RX,
                        CY_RETARGET_IO_BAUDRATE);

    cybt_platform_config_init(&bt_platform_cfg_settings);
    printf("**********AnyCloud Example*****************\n");
    printf("**** Battery Server Application Start ****\n");

    /* Register call back and configuration with stack */
    result = wiced_bt_stack_init (app_bt_management_callback, &wiced_bt_cfg_settings);

    /* Check if stack initialization was successful */
    if( WICED_BT_SUCCESS == result)
    {
        printf("Bluetooth Stack Initialization Successful \n");
    }
    else
    {
        printf("Bluetooth Stack Initialization failed!! \n");
        CY_ASSERT(0);
    }

    /* Start the FreeRTOS scheduler */
    vTaskStartScheduler() ;

    /* Should never get here */
    CY_ASSERT(0) ;
}

/**************************************************************************************************
* Function Name: app_bt_management_callback()
***************************************************************************************************
* Summary:
*   This is a Bluetooth stack event handler function to receive management events from
*   the BLE stack and process as per the application.
*
* Parameters:
*   wiced_bt_management_evt_t event             : BLE event code of one byte length
*   wiced_bt_management_evt_data_t *p_event_data: Pointer to BLE management event structures
*
* Return:
*  wiced_result_t: Error code from WICED_RESULT_LIST or BT_RESULT_LIST
*
*************************************************************************************************/
wiced_result_t app_bt_management_callback(wiced_bt_management_evt_t event, wiced_bt_management_evt_data_t *p_event_data)
{
    wiced_result_t status = WICED_BT_SUCCESS;
    wiced_bt_device_address_t bda = { 0 };
    wiced_bt_ble_advert_mode_t *p_adv_mode = NULL;

    switch (event)
    {
        case BTM_ENABLED_EVT:
            /* Bluetooth Controller and Host Stack Enabled */

            if (WICED_BT_SUCCESS == p_event_data->enabled.status)
            {
                /* Bluetooth is enabled */
                wiced_bt_dev_read_local_addr(bda);
                printf("Local Bluetooth Address: ");
                print_bd_address(bda);

                /* Perform application-specific initialization */
                ble_app_init();
            }

            break;

        case BTM_BLE_ADVERT_STATE_CHANGED_EVT:

            /* Advertisement State Changed */
            p_adv_mode = &p_event_data->ble_advert_state_changed;
            printf("Advertisement State Change: %s\n", get_bt_advert_mode_name(*p_adv_mode));

            if (BTM_BLE_ADVERT_OFF == *p_adv_mode)
            {
                /* Advertisement Stopped */
                printf("Advertisement stopped\n");

                /* Check connection status after advertisement stops */
                if(bt_connection_id == 0)
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
                printf("Advertisement started\n");
                app_bt_adv_conn_state = APP_BT_ADV_ON_CONN_OFF;
            }

            /* Update Advertisement LED to reflect the updated state */
            adv_led_update();
            break;

        default:
            printf("Unhandled Bluetooth Management Event: 0x%x %s\n", event, get_bt_event_name(event));
            break;
    }

    return status;
}

/**************************************************************************************************
* Function Name: ble_app_init()
***************************************************************************************************
* Summary:
*   This function handles application level initialization tasks and is called from the BT
*   management callback once the BLE stack enabled event (BTM_ENABLED_EVT) is triggered
*   This function is executed in the BTM_ENABLED_EVT management callback.
*
* Parameters:
*   None
*
* Return:
*  None
*
*************************************************************************************************/
static void ble_app_init(void)
{
    cy_rslt_t rslt = CY_RSLT_SUCCESS;
    wiced_bt_gatt_status_t status = WICED_BT_GATT_SUCCESS;
    wiced_result_t result;

    printf("\n***********************************************\n");
    printf("**Discover device with \"Battery Server\" name*\n");
    printf("***********************************************\n\n");

    /* Initialize the PWM used for Advertising LED */
    rslt = cyhal_pwm_init(&adv_led_pwm, ADV_LED_GPIO , NULL);

    /* PWM init failed. Stop program execution */
    if (CY_RSLT_SUCCESS !=  rslt)
    {
        printf("Advertisement LED PWM Initialization has failed! \n");
        CY_ASSERT(0);
    }

    /* Initialize the timer used Battery Level */
    rslt = cy_rtos_init_timer(&batt_level_timer, CY_TIMER_TYPE_PERIODIC, batt_level_timer_cb , 0);

    /* Timer init failed. Stop program execution */
    if (CY_RSLT_SUCCESS !=  rslt)
    {
        printf("Battery Level Timer Initialization has failed! \n");
        CY_ASSERT(0);
    }

    /* Disable pairing for this application */
    wiced_bt_set_pairable_mode(WICED_FALSE, 0);

    /* Set Advertisement Data */
    ble_app_set_advertisement_data();

    /* Register with BT stack to receive GATT callback */
    status = wiced_bt_gatt_register(ble_app_gatt_event_handler);
    printf("GATT event Handler registration status: %s \n",get_bt_gatt_status_name(status));

    /* Initialize GATT Database */
    status = wiced_bt_gatt_db_init(gatt_database, gatt_database_len, NULL);
    printf("GATT database initialization status: %s \n",get_bt_gatt_status_name(status));

    /* Start Undirected LE Advertisements on device startup.
     * The corresponding parameters are contained in 'app_bt_cfg.c' */
    result = wiced_bt_start_advertisements(BTM_BLE_ADVERT_UNDIRECTED_HIGH, 0, NULL);
    if(WICED_BT_SUCCESS != result)
    {
        printf("Advertisement cannot start because of error: %d \n",result);
        CY_ASSERT(0);
    }

    /* Start battery level timer */
    batt_level_init();
}

/**************************************************************************************************
* Function Name: ble_app_gatt_event_handler()
***************************************************************************************************
* Summary:
*   This function handles GATT events from the BT stack.
*
* Parameters:
*   wiced_bt_gatt_evt_t event                   : BLE GATT event code of one byte length
*   wiced_bt_gatt_event_data_t *p_event_data    : Pointer to BLE GATT event structures
*
* Return:
*  wiced_bt_gatt_status_t: See possible status codes in wiced_bt_gatt_status_e in wiced_bt_gatt.h
*
**************************************************************************************************/
static wiced_bt_gatt_status_t ble_app_gatt_event_handler(wiced_bt_gatt_evt_t event, wiced_bt_gatt_event_data_t *p_event_data)
{
    wiced_bt_gatt_status_t status = WICED_BT_GATT_ERROR;
    wiced_bt_gatt_attribute_request_t *p_attr_req = NULL;

    /* Call the appropriate callback function based on the GATT event type, and pass the relevant event
     * parameters to the callback function */
    switch ( event )
    {
        case GATT_CONNECTION_STATUS_EVT:
            status = ble_app_connect_callback( &p_event_data->connection_status );
            break;

        case GATT_ATTRIBUTE_REQUEST_EVT:
            p_attr_req = &p_event_data->attribute_request;
            status = ble_app_server_callback( p_attr_req->conn_id, p_attr_req->request_type, &p_attr_req->data );
            break;

        default:
            status = WICED_BT_GATT_SUCCESS;
            break;
    }

    return status;
}

/**************************************************************************************************
* Function Name: ble_app_set_advertisement_data()
***************************************************************************************************
* Summary:
*   This function configures the advertisement packet data
*
**************************************************************************************************/
static void ble_app_set_advertisement_data(void)
{
    wiced_bt_ble_advert_elem_t adv_elem[3] = { 0 };
    uint8_t adv_flag = BTM_BLE_GENERAL_DISCOVERABLE_FLAG | BTM_BLE_BREDR_NOT_SUPPORTED;
    uint8_t adv_appearance[] = { BIT16_TO_8( APPEARANCE_GENERIC_KEYRING ) };
    uint8_t num_elem = 0;

    /* Advertisement Element for Flags */
    adv_elem[num_elem].advert_type = BTM_BLE_ADVERT_TYPE_FLAG;
    adv_elem[num_elem].len = sizeof(uint8_t);
    adv_elem[num_elem].p_data = &adv_flag;
    num_elem++;

    /* Advertisement Element for Name */
    adv_elem[num_elem].advert_type = BTM_BLE_ADVERT_TYPE_NAME_COMPLETE;
    adv_elem[num_elem].len = app_gap_device_name_len;
    adv_elem[num_elem].p_data = app_gap_device_name;
    num_elem++;

    /* Advertisement Element for Appearance */
    adv_elem[num_elem].advert_type = BTM_BLE_ADVERT_TYPE_APPEARANCE;
    adv_elem[num_elem].len = sizeof(adv_appearance);
    adv_elem[num_elem].p_data = adv_appearance;
    num_elem++;

    /* Set Raw Advertisement Data */
    wiced_bt_ble_set_raw_advertisement_data(num_elem, adv_elem);
}

/**************************************************************************************************
* Function Name: ble_app_get_value()
***************************************************************************************************
* Summary:
*   This function handles reading of the attribute value from the GATT database and passing the
*   data to the BT stack. The value read from the GATT database is stored in a buffer whose
*   starting address is passed as one of the function parameters
*
* Parameters:
*   wiced_bt_gatt_write_t *p_read_req           : Pointer that contains details of Read Request
*                                                 including the attribute handle
*
* Return:
*   wiced_bt_gatt_status_t: See possible status codes in wiced_bt_gatt_status_e in wiced_bt_gatt.h
*
**************************************************************************************************/
static wiced_bt_gatt_status_t ble_app_get_value(wiced_bt_gatt_read_t *p_read_req)
{
    wiced_bool_t isHandleInTable = WICED_FALSE;
    wiced_bt_gatt_status_t res = WICED_BT_GATT_INVALID_HANDLE;
    uint16_t attr_handle = p_read_req->handle;
    uint8_t *p_val = p_read_req->p_val;
    uint16_t max_len = *p_read_req->p_val_len;
    uint16_t *p_len = p_read_req->p_val_len;

    /* Check for a matching handle entry */
    for (int i = 0; i < app_gatt_db_ext_attr_tbl_size; i++)
    {
        if (app_gatt_db_ext_attr_tbl[i].handle == attr_handle)
        {
            /* Detected a matching handle in external lookup table */
            isHandleInTable = WICED_TRUE;

            /* Check if the buffer has space to store the data */
            if (app_gatt_db_ext_attr_tbl[i].cur_len <= max_len)
            {
                /* Value fits within the supplied buffer; copy over the value */
                *p_len = app_gatt_db_ext_attr_tbl[i].cur_len;
                memcpy(p_val, app_gatt_db_ext_attr_tbl[i].p_data, app_gatt_db_ext_attr_tbl[i].cur_len);
                res = WICED_BT_GATT_SUCCESS;

                /* Add code for any action required when a particular attribute is read */
                switch ( attr_handle )
                {
                    case HDLC_GAP_DEVICE_NAME_VALUE:
                        break;
                    case HDLC_GAP_APPEARANCE_VALUE:
                        break;
                }
            }
            else
            {
                /* Value to read will not fit within the buffer */
                res = WICED_BT_GATT_INVALID_ATTR_LEN;
            }
            break;
        }
    }

    if (!isHandleInTable)
    {
        /* Add code to read value for handles not contained within generated lookup table.
         * This is a custom logic that depends on the application, and is not used in the
         * current application. If the value for the current handle is successfully read in the
         * below code snippet, then set the result using:
         * res = WICED_BT_GATT_SUCCESS; */
        switch ( attr_handle )
        {
            default:
                /* The read operation was not performed for the indicated handle */
                printf("Read Request to Invalid Handle: 0x%x\n", attr_handle);
                res = WICED_BT_GATT_READ_NOT_PERMIT;
                break;
        }
    }

    return res;
}


/**************************************************************************************************
* Function Name: ble_app_write_handler()
***************************************************************************************************
* Summary:
*   This function handles Write Requests received from the client device
*
* Parameters:
*   wiced_bt_gatt_write_t *p_write_req          : Pointer that contains details of Write Request
*                                                 including the attribute handle
*   uint16_t conn_id                            : Connection ID
*
* Return:
*  wiced_bt_gatt_status_t: See possible status codes in wiced_bt_gatt_status_e in wiced_bt_gatt.h
*
**************************************************************************************************/
static wiced_bt_gatt_status_t ble_app_write_handler(wiced_bt_gatt_write_t *p_write_req, uint16_t conn_id)
{
    wiced_bt_gatt_status_t status = WICED_BT_GATT_SUCCESS;

    /* Enable/Disable Notification on request received */
    app_bas_battery_level_client_char_config[0] = *(p_write_req->p_val);

    return status;
}

/**************************************************************************************************
* Function Name: ble_app_read_handler()
***************************************************************************************************
* Summary:
*   This function handles Read Requests received from the client device
*
* Parameters:
*   wiced_bt_gatt_write_t *p_read_req           : Pointer that contains details of Read Request
*                                                 including the attribute handle
*   uint16_t conn_id                            : Connection ID
*
* Return:
*  wiced_bt_gatt_status_t: See possible status codes in wiced_bt_gatt_status_e in wiced_bt_gatt.h
*
**************************************************************************************************/
static wiced_bt_gatt_status_t ble_app_read_handler( wiced_bt_gatt_read_t *p_read_req, uint16_t conn_id )
{
    wiced_bt_gatt_status_t status = WICED_BT_GATT_INVALID_HANDLE;

    /* Attempt to perform the Read Request */
    status = ble_app_get_value(p_read_req);

    return status;
}

/**************************************************************************************************
* Function Name: ble_app_connect_callback()
***************************************************************************************************
* Summary:
*   This callback function handles connection status changes.
*
* Parameters:
*   wiced_bt_gatt_connection_status_t *p_conn_status  : Pointer to data that has connection details
*
* Return:
*  wiced_bt_gatt_status_t: See possible status codes in wiced_bt_gatt_status_e in wiced_bt_gatt.h
*
**************************************************************************************************/
static wiced_bt_gatt_status_t ble_app_connect_callback(wiced_bt_gatt_connection_status_t *p_conn_status)
{
    wiced_bt_gatt_status_t status = WICED_BT_GATT_ERROR;
    wiced_result_t result;

    if ( NULL != p_conn_status )
    {
        if ( p_conn_status->connected )
        {
            /* Device has connected */
            printf("Connected : BDA " );
            print_bd_address(p_conn_status->bd_addr);
            printf("Connection ID '%d'\n", p_conn_status->conn_id );

            /* Store the connection ID */
            bt_connection_id = p_conn_status->conn_id;

            /* Update the adv/conn state */
            app_bt_adv_conn_state = APP_BT_ADV_OFF_CONN_ON;

        }
        else
        {
            /* Device has disconnected */
            printf("Disconnected : BDA " );
            print_bd_address(p_conn_status->bd_addr);
            printf("Connection ID '%d', Reason '%s'\n", p_conn_status->conn_id, get_bt_gatt_disconn_reason_name(p_conn_status->reason) );

            /* Set the connection id to zero to indicate disconnected state */
            bt_connection_id = 0;

            /* Restart the advertisements */
            result = wiced_bt_start_advertisements(BTM_BLE_ADVERT_UNDIRECTED_HIGH, 0, NULL);
            if(WICED_BT_SUCCESS != result)
            {
                printf("Advertisement cannot start because of error: %d \n",result);
                CY_ASSERT(0);
            }

            /* Update the adv/conn state */
            app_bt_adv_conn_state = APP_BT_ADV_ON_CONN_OFF;

        }

        /* Update Advertisement LED to reflect the updated state */
        adv_led_update();

        status = WICED_BT_GATT_SUCCESS;
    }

    return status;
}

/**************************************************************************************************
* Function Name: ble_app_server_callback()
***************************************************************************************************
* Summary:
*   This function handles GATT server events from the BT stack.
*
* Parameters:
*   uint16_t conn_id                            : Connection ID
*   wiced_bt_gatt_request_type_t type           : Type of GATT server event
*   wiced_bt_gatt_request_data_t *p_data        : Pointer to GATT server event data
*
* Return:
*  wiced_bt_gatt_status_t: See possible status codes in wiced_bt_gatt_status_e in wiced_bt_gatt.h
*
**************************************************************************************************/
static wiced_bt_gatt_status_t ble_app_server_callback(uint16_t conn_id, wiced_bt_gatt_request_type_t type, wiced_bt_gatt_request_data_t *p_data)
{
    wiced_bt_gatt_status_t status = WICED_BT_GATT_ERROR;

    switch ( type )
    {
        case GATTS_REQ_TYPE_READ:
            /* Attribute read request */
            status = ble_app_read_handler( &p_data->read_req, conn_id );
            break;
        case GATTS_REQ_TYPE_WRITE:
            /* Attribute write request */
            status = ble_app_write_handler( &p_data->write_req, conn_id );
            break;
    }

    return status;
}

/*******************************************************************************
* Function Name: batt_level_init()
********************************************************************************
*
* Summary:
*   This function Starts the timer for updating Battery Level
*
*******************************************************************************/
static void batt_level_init(void)
{
    cy_rslt_t rslt = CY_RSLT_SUCCESS;
    rslt = cy_rtos_start_timer(&batt_level_timer, BATTERY_LEVEL_UPDATE_MS);
    if (CY_RSLT_SUCCESS != rslt)
    {
        printf("Failed to Start Battery level timer!\n");
        CY_ASSERT(0);
    }
    printf("Battery level timer started!\n");

}

/*******************************************************************************
* Function Name: adv_led_update()
********************************************************************************
*
* Summary:
*   This function updates the advertising LED state based on BLE advertising/
*   connection state
*
*******************************************************************************/
static void adv_led_update(void)
{
    cy_rslt_t rslt = CY_RSLT_SUCCESS;
    cy_rslt_t start_rslt = CY_RSLT_SUCCESS;
    /* Stop the advertising led pwm */
    rslt = cyhal_pwm_stop(&adv_led_pwm);
    if (CY_RSLT_SUCCESS != rslt)
    {
         printf("Failed to stop PWM !!\n");
    }

    /* Update LED state based on BLE advertising/connection state.
     * LED OFF for no advertisement/connection, LED blinking for advertisement
     * state, and LED ON for connected state  */
    switch(app_bt_adv_conn_state)
    {
        case APP_BT_ADV_OFF_CONN_OFF:
            rslt = cyhal_pwm_set_duty_cycle(&adv_led_pwm, LED_OFF_DUTY_CYCLE, ADV_LED_PWM_FREQUENCY);
            break;

        case APP_BT_ADV_ON_CONN_OFF:
            rslt = cyhal_pwm_set_duty_cycle(&adv_led_pwm, LED_BLINKING_DUTY_CYCLE, ADV_LED_PWM_FREQUENCY);
            break;

        case APP_BT_ADV_OFF_CONN_ON:
            rslt = cyhal_pwm_set_duty_cycle(&adv_led_pwm, LED_ON_DUTY_CYCLE, ADV_LED_PWM_FREQUENCY);
            break;

        default:
            /* LED OFF for unexpected states */
            rslt = cyhal_pwm_set_duty_cycle(&adv_led_pwm, LED_OFF_DUTY_CYCLE, ADV_LED_PWM_FREQUENCY);
            break;
    }
    /* Check if update to PWM parameters is successful*/
    if (CY_RSLT_SUCCESS != rslt)
    {
         printf("Failed to set duty cycle parameters!!\n");
    }

    /* Start the advertising led pwm */
    start_rslt = cyhal_pwm_start(&adv_led_pwm);

    /* Check if PWM started successfully */
    if (CY_RSLT_SUCCESS != start_rslt)
    {
         printf("Failed to start PWM !!\n");
    }
}

/*******************************************************************************
* Function Name: batt_level_timer_cb()
********************************************************************************
*
* Summary:
*   This timer callback function reduces the battery level by 2% every second
*   and sends notification to connected battery client
*
* Parameters:
*   uint32_t arg - The argument parameter is not used in this callback
*
* Return:
*   None
*
*******************************************************************************/
static void batt_level_timer_cb(cy_timer_callback_arg_t cb_params)
{

    /* Battery level is read from gatt db and is reduced by 2 percent
     * Initialized again to 100 once it reaches 0*/
    if (0 == app_bas_battery_level[0])
    {
        app_bas_battery_level[0] = 100;
    }
    else
    {
        app_bas_battery_level[0] = app_bas_battery_level[0] - 2;
    }

    if (bt_connection_id)
    {
        if(app_bas_battery_level_client_char_config[0] & GATT_CLIENT_CONFIG_NOTIFICATION)
        {
            wiced_bt_gatt_send_notification(bt_connection_id, HDLC_BAS_BATTERY_LEVEL_VALUE,
                                            1, app_bas_battery_level);
            printf("Send Notification: Sending Battery level: %u\n",app_bas_battery_level[0]);
        }
    }
}


/* [] END OF FILE */
