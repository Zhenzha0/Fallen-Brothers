 /******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 * (c) CG2028 Teaching Team
 ******************************************************************************/


/*--------------------------- Includes ---------------------------------------*/
#include "main.h"
#include "../../Drivers/BSP/B-L4S5I-IOT01/stm32l4s5i_iot01_accelero.h"
#include "../../Drivers/BSP/B-L4S5I-IOT01/stm32l4s5i_iot01_tsensor.h"
#include "../../Drivers/BSP/B-L4S5I-IOT01/stm32l4s5i_iot01_gyro.h"
#include "../../Drivers/BSP/B-L4S5I-IOT01/stm32l4s5i_iot01_psensor.h"

#include "stdio.h"
#include "string.h"
#include <sys/stat.h>
#include <math.h>
#include "wifi.h"

static void UART1_Init(void);
static void Buzzer_GPIO_Init(void);
static void send_ntfy_alert(const char *title, const char *priority, const char *tags, const char *body);

extern void initialise_monitor_handles(void);	// for semi-hosting support (printf). Will not be required if transmitting via UART

extern int mov_avg(int N, int* accel_buff); // asm implementation

int mov_avg_C(int N, int* accel_buff); // Reference C implementation

UART_HandleTypeDef huart1;

/* ----------------------- Fall detection types/helpers ----------------------- */
typedef enum {
    normal = 0,
    freefall,
    impact,
    possible_fall,
    fall
} fall_state_t;

static const char* state_str(fall_state_t s)
{
    switch (s) {
        case normal:        return "normal";
        case freefall:      return "freefall";
        case impact:        return "impact";
        case possible_fall: return "possible_fall";
        case fall:          return "FALL";
        default:            return "unknown";
    }
}

int main(void)
{
	const int N=4;

	/* Reset of all peripherals, Initializes the Flash interface and the Systick. */
	HAL_Init();

	/* UART initialization  */
	UART1_Init();

	/* Peripheral initializations using BSP functions */
	BSP_LED_Init(LED2);
	BSP_ACCELERO_Init();
	BSP_GYRO_Init();
	BSP_PSENSOR_Init();

	/* Buzzer GPIO initialization (Arduino D3 -> PB0) */
	Buzzer_GPIO_Init();

	/*Set the initial LED state to off*/
	BSP_LED_Off(LED2);

	int accel_buff_x[4]={0};
	int accel_buff_y[4]={0};
	int accel_buff_z[4]={0};
	int i=0;
	const int delay_ms = 20;

    /* UART throttling: keep Part 1 text exactly, but print less frequently */
    uint32_t last_uart_ms = 0;
    uint32_t uart_period_ms = 100000;   // print once per second

    /* LED timing */
    uint32_t last_led_toggle_ms = 0;
    uint32_t led_period_ms = 500;     // normal blink rate (ms)

    /* Fall detection parameters */
    const float G = 9.8f;

    /* Use RAW magnitude for freefall/impact triggers */
    const float FREEFALL_THR = 0.8f * G;   //  tuned
    const float IMPACT_THR   = 1.0f * G;   // tuned

    /* Still / Long-lie uses filtered accel magnitude + gyro magnitude (gyro in dps) */
    const float STILL_GYRO_THR   = 3.0f;      // ~3 dps, scaled after fixing gyro units
    const float STILL_ACCEL_LOW  = 0.8f * G;
    const float STILL_ACCEL_HIGH = 1.2f * G;

    /* Moving / filter value to test if the subject is moving again in possible fall stage (gyro in dps) */
    const float MOVE_GYRO_THR = 5.0f;   // bigger than STILL_GYRO_THR (hysteresis), scaled after unit fix
    const float MOVE_ACCEL_DEV = 0.35f * G; // deviation from 1g considered moving (also hysteresis)


    /* Debounce / time windows */
    int freefall_count = 0;
    const int FREEFALL_N = 2; // need 2 consecutive samples below threshold

    uint32_t freefall_start_ms = 0; // time when we entered freefall
    const uint32_t FREEFALL_TIMEOUT_MS = 1500; // impact should follow soon
    float freefall_start_alt = 0.0f; // altitude when freefall began

    const int LONGLIE_SEC = 5; // adjust here if you want to tune
    const uint32_t LONGLIE_MS = LONGLIE_SEC * 1000; // long-lie duration in ms
    uint32_t longlie_start_ms = 0; // timestamp when stillness period starts

    /* Moving debounce: time-based (wall-clock) instead of sample-count */
    uint32_t move_start_ms = 0;        // timestamp when movement starts
    const int MOVE_SEC = 2;            // require 2 s of continuous movement
    const uint32_t MOVE_MS = MOVE_SEC * 1000;

    //recover debounce
    const int FMOVE_SEC = 3;            // require 3 s of continuous movement
    const uint32_t FMOVE_MS = FMOVE_SEC * 1000;

    /* State machine */
    fall_state_t state = normal;
    fall_state_t prev_state = normal;

    /* ---------- Altitude tracking ---------- */
    float pressure_ema = 0.0f;
    float ref_pressure = 0.0f;
    const float ALT_EMA_ALPHA = 0.1f;
    const float ALT_HYSTERESIS_M = 0.01f;

    /* Take initial reference pressure from first barometer read after sensor settles */
    HAL_Delay(300);
    ref_pressure = BSP_PSENSOR_ReadPressureHP();
    pressure_ema = ref_pressure;

    /* Wi-Fi initialization */
    char uart_buf[200];

    sprintf(uart_buf, "Initializing Wi-Fi module...\r\n");
    HAL_UART_Transmit(&huart1, (uint8_t*)uart_buf, strlen(uart_buf), HAL_MAX_DELAY);

    if (WIFI_Init() != WIFI_STATUS_OK) {
        sprintf(uart_buf, "ERROR: Wi-Fi init failed!\r\n");
        HAL_UART_Transmit(&huart1, (uint8_t*)uart_buf, strlen(uart_buf), HAL_MAX_DELAY);
    } else {
        sprintf(uart_buf, "Wi-Fi module initialized OK.\r\n");
        HAL_UART_Transmit(&huart1, (uint8_t*)uart_buf, strlen(uart_buf), HAL_MAX_DELAY);
    }

    sprintf(uart_buf, "Connecting to Wi-Fi...\r\n");
    HAL_UART_Transmit(&huart1, (uint8_t*)uart_buf, strlen(uart_buf), HAL_MAX_DELAY);

    HAL_Delay(2000);

    if (WIFI_Connect("Yang", "password", WIFI_ECN_WPA_WPA2_PSK) != WIFI_STATUS_OK) {
        sprintf(uart_buf, "ERROR: Wi-Fi connect failed!\r\n");
        HAL_UART_Transmit(&huart1, (uint8_t*)uart_buf, strlen(uart_buf), HAL_MAX_DELAY);
    } else {
        sprintf(uart_buf, "Wi-Fi connected!\r\n");
        HAL_UART_Transmit(&huart1, (uint8_t*)uart_buf, strlen(uart_buf), HAL_MAX_DELAY);
    }

	while (1)
	{



		int16_t accel_data_i16[3] = { 0 };			// array to store the x, y and z readings of accelerometer
		/********Function call to read accelerometer values*********/
		BSP_ACCELERO_AccGetXYZ(accel_data_i16);

		//Copy the values over to a circular style buffer
		accel_buff_x[i%4]=accel_data_i16[0]; //acceleration along X-Axis
		accel_buff_y[i%4]=accel_data_i16[1]; //acceleration along Y-Axis
		accel_buff_z[i%4]=accel_data_i16[2]; //acceleration along Z-Axis


		// ********* Read gyroscope values *********/
		float gyro_data[3]={0.0};
		float* ptr_gyro=gyro_data;
		BSP_GYRO_GetXYZ(ptr_gyro);

		//The output of gyro is provided by BSP in mdps (millidegrees per second).
		//Convert to standard dps (degrees per second).
		float gyro_velocity[3]={0.0};
		gyro_velocity[0]= gyro_data[0] / 1000.0f;
		gyro_velocity[1]= gyro_data[1] / 1000.0f;
		gyro_velocity[2]= gyro_data[2] / 1000.0f;


		//Preprocessing the filtered outputs  The same needs to be done for the output from the C program as well
		float accel_filt_asm[3]={0}; // final value of filtered acceleration values

		accel_filt_asm[0]= (float)mov_avg(N,accel_buff_x) * (9.8/1000.0f);
		accel_filt_asm[1]= (float)mov_avg(N,accel_buff_y) * (9.8/1000.0f);
		accel_filt_asm[2]= (float)mov_avg(N,accel_buff_z) * (9.8/1000.0f);


		//Preprocessing the filtered outputs  The same needs to be done for the output from the assembly program as well
		float accel_filt_c[3]={0};

		accel_filt_c[0]=(float)mov_avg_C(N,accel_buff_x) * (9.8/1000.0f);
		accel_filt_c[1]=(float)mov_avg_C(N,accel_buff_y) * (9.8/1000.0f);
		accel_filt_c[2]=(float)mov_avg_C(N,accel_buff_z) * (9.8/1000.0f);

		//gryo magnitude caculation
		 float gyro_mag = sqrtf(gyro_velocity[0]*gyro_velocity[0] +
							    gyro_velocity[1]*gyro_velocity[1] +
							    gyro_velocity[2]*gyro_velocity[2]);

		 /* Filtered magnitude for "still / long lie" */
		float filt_accel_mag = sqrtf(accel_filt_asm[0]*accel_filt_asm[0] +
									 accel_filt_asm[1]*accel_filt_asm[1] +
									 accel_filt_asm[2]*accel_filt_asm[2]);




        /* ------------------- Altitude tracking ------------------- */

        float pressure = BSP_PSENSOR_ReadPressureHP();
        pressure_ema = ALT_EMA_ALPHA * pressure + (1.0f - ALT_EMA_ALPHA) * pressure_ema;

        float altitude = 44330.0f * (1.0f - powf(pressure_ema / ref_pressure, 0.190284f));

        /* ------------------- Fall detection  ------------------- */
        /* helper: still/lying condition uses filtered accel + gyro */
        int is_still = (gyro_mag < STILL_GYRO_THR) &&
                       (filt_accel_mag > STILL_ACCEL_LOW) &&
                       (filt_accel_mag < STILL_ACCEL_HIGH);

        /* helper to see if the person started moving, calculate acceleration's absolute deviation from G */
        float accel_dev = fabsf(filt_accel_mag - G);  // how far from 1g
        int is_moving = (gyro_mag > MOVE_GYRO_THR) || (accel_dev > MOVE_ACCEL_DEV);

        uint32_t now = HAL_GetTick();

        switch (state)
        {
            case normal:
                /* Look for freefall using RAW magnitude only */
                if (filt_accel_mag < FREEFALL_THR) {
                    freefall_count++;
                    if (freefall_count >= FREEFALL_N) {
                        state = freefall;
                        freefall_count = 0;
                        freefall_start_ms = now;
                        freefall_start_alt = altitude;
                    }
                } else {
                    freefall_count = 0;
                }
                break;

            case freefall:
                /* After freefall, look for impact; must be in descending phase */
                if (filt_accel_mag > IMPACT_THR) {
                	state = impact;

                } else if ((now - freefall_start_ms) > FREEFALL_TIMEOUT_MS) {
                    /* No impact quickly -> cancel */
                    state = normal;
                }
                break;

            case impact:
            {
                float alt_drop = freefall_start_alt - altitude;
                char alt_msg[100];
                sprintf(alt_msg, "IMPACT: alt_drop=%.4fm (thr=%.4fm)\r\n", alt_drop, ALT_HYSTERESIS_M);
                HAL_UART_Transmit(&huart1, (uint8_t*)alt_msg, strlen(alt_msg), HAL_MAX_DELAY);

                if (alt_drop >= ALT_HYSTERESIS_M)
                {
				    state = possible_fall;
				    longlie_start_ms = 0;

                } else {
				    state = normal;

                }
            }


                break;

            case possible_fall:
                /* Confirm long-lie for 5s using wall-clock time */
                if (is_still) {
                    if (longlie_start_ms == 0) {
                        longlie_start_ms = now;  /* first still sample */
                    }
                    if ((now - longlie_start_ms) >= LONGLIE_MS) {
                        state = fall;
                    }
                } else if (is_moving){
                    /* Moved again => not a long lie.
                     * Use wall-clock time: need MOVE_MS of continuous movement. */
                    if (move_start_ms == 0) {
                        move_start_ms = now;       /* first moving sample */
                    }

                    if ((now - move_start_ms) >= MOVE_MS) {
                        state = normal;
                        move_start_ms = 0;
                        longlie_start_ms = 0;
                    }
                } else {
                    /* Neither clearly still nor clearly moving: treat as noise and reset movement timer */
                    move_start_ms = 0;
                }
                break;

            case fall:
                /* Remain in fall until movement returns */
                if (is_moving) {
                	if (move_start_ms == 0) {
						move_start_ms = now;       /* first moving sample */
					}

					if ((now - move_start_ms) >= FMOVE_MS) {
						state = normal;
						move_start_ms = 0;
						longlie_start_ms = 0;
					}
                }
                break;

            default:
                state = normal;
                break;
        }

        /* ------------------- Event-driven state change UART (NOT inside main UART block) ------------------- */
        if (state != prev_state)
        {
            char msg[220];
            sprintf(msg,
                    "STATE CHANGE: %s -> %s | filt_am=%.2f | gmag=%.2f\r\n",
                    state_str(prev_state), state_str(state),
                    filt_accel_mag, gyro_mag);
            HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);


            if (state == fall)
            {
                send_ntfy_alert("Fall Alert", "urgent", "warning",
                                "ZHENZHAO HAS FALLEN! Send Help ASAP!!!!");
            }

            if (prev_state == fall && state == normal)
            {
                send_ntfy_alert("Recovery Alert", "default", "white_check_mark",
                                "Zhenzhao has recovered. No further action needed.");
            }

            prev_state = state;
        }

        /* ------------------- LED behaviour ------------------- */
        led_period_ms = (state == fall) ? 100 : 500; // fast blink only for confirmed fall
        if (now - last_led_toggle_ms >= led_period_ms) {
            BSP_LED_Toggle(LED2);
            last_led_toggle_ms = now;
        }

        /* ------------------- Buzzer behaviour ------------------- */
        if (state == fall) {
        	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
        } else {
        	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
        }







		/***************************UART transmission*******************************************/
		char buffer[150]; // Create a buffer large enough to hold the text

		/******Transmitting results of C execution over UART*********/
		if(i>=3 && (now - last_uart_ms >= uart_period_ms))
		{
			last_uart_ms = now;
			// 1. First printf() Equivalent
			sprintf(buffer, "Results of C execution for filtered accelerometer readings:\r\n");
			HAL_UART_Transmit(&huart1, (uint8_t*)buffer, strlen(buffer), HAL_MAX_DELAY);

			// 2. Second printf() (with Floats) Equivalent
			// Note: Requires -u _printf_float to be enabled in Linker settings
			sprintf(buffer, "Averaged X : %f; Averaged Y : %f; Averaged Z : %f;\r\n",
					accel_filt_c[0], accel_filt_c[1], accel_filt_c[2]);
			HAL_UART_Transmit(&huart1, (uint8_t*)buffer, strlen(buffer), HAL_MAX_DELAY);

			/******Transmitting results of asm execution over UART*********/

			// 1. First printf() Equivalent
			sprintf(buffer, "Results of assembly execution for filtered accelerometer readings:\r\n");
			HAL_UART_Transmit(&huart1, (uint8_t*)buffer, strlen(buffer), HAL_MAX_DELAY);

			// 2. Second printf() (with Floats) Equivalent
			// Note: Requires -u _printf_float to be enabled in Linker settings
			sprintf(buffer, "Averaged X : %f; Averaged Y : %f; Averaged Z : %f;\r\n",
					accel_filt_asm[0], accel_filt_asm[1], accel_filt_asm[2]);
			HAL_UART_Transmit(&huart1, (uint8_t*)buffer, strlen(buffer), HAL_MAX_DELAY);

			/******Transmitting Gyroscope readings over UART*********/

			// 1. First printf() Equivalent
			sprintf(buffer, "Gyroscope sensor readings:\r\n");
			HAL_UART_Transmit(&huart1, (uint8_t*)buffer, strlen(buffer), HAL_MAX_DELAY);

			// 2. Second printf() (with Floats) Equivalent
			// Note: Requires -u _printf_float to be enabled in Linker settings
			sprintf(buffer, "Averaged X : %f; Averaged Y : %f; Averaged Z : %f;\r\n",
					gyro_velocity[0], gyro_velocity[1], gyro_velocity[2]);
			HAL_UART_Transmit(&huart1, (uint8_t*)buffer, strlen(buffer), HAL_MAX_DELAY);

		}

		HAL_Delay(delay_ms);

		i++;

		// ********* Fall detection *********/
		// write your program from here:


	}


}





static void send_ntfy_alert(const char *title, const char *priority, const char *tags, const char *body)
{
    uint8_t ntfy_ip[4];
    uint16_t sent = 0;

    if (WIFI_GetHostAddress("ntfy.sh", ntfy_ip) != WIFI_STATUS_OK)
        return;
    if (WIFI_OpenClientConnection(0, WIFI_TCP_PROTOCOL, "ntfy", ntfy_ip, 80, 0) != WIFI_STATUS_OK)
        return;

    char http_req[512];
    int body_len = strlen(body);
    sprintf(http_req,
            "POST /cg2028-fall-alert HTTP/1.1\r\n"
            "Host: ntfy.sh\r\n"
            "Title: %s\r\n"
            "Priority: %s\r\n"
            "Tags: %s\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: %d\r\n"
            "\r\n"
            "%s",
            title, priority, tags, body_len, body);

    WIFI_SendData(0, (uint8_t *)http_req, strlen(http_req), &sent, 5000);
    HAL_Delay(500);
    WIFI_CloseClientConnection(0);

    char log[80];
    sprintf(log, "ntfy notification sent (%d bytes)\r\n", sent);
    HAL_UART_Transmit(&huart1, (uint8_t*)log, strlen(log), HAL_MAX_DELAY);
}

int mov_avg_C(int N, int* accel_buff)
{ 	// The implementation below is inefficient and meant only for verifying your results.
	int result=0;
	for(int i=0; i<N;i++)
	{
		result+=accel_buff[i];
	}

	result=result/4;

	return result;
}

static void Buzzer_GPIO_Init(void)
{
	__HAL_RCC_GPIOB_CLK_ENABLE();

	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = GPIO_PIN_0;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	/* Ensure buzzer is off at startup */
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
}

static void UART1_Init(void)
{
        /* Pin configuration for UART. BSP_COM_Init() can do this automatically */
        __HAL_RCC_GPIOB_CLK_ENABLE();
         __HAL_RCC_USART1_CLK_ENABLE();

        GPIO_InitTypeDef GPIO_InitStruct = {0};
        GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
        GPIO_InitStruct.Pin = GPIO_PIN_7|GPIO_PIN_6;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

        /* Configuring UART1 */
        huart1.Instance = USART1;
        huart1.Init.BaudRate = 115200;
        huart1.Init.WordLength = UART_WORDLENGTH_8B;
        huart1.Init.StopBits = UART_STOPBITS_1;
        huart1.Init.Parity = UART_PARITY_NONE;
        huart1.Init.Mode = UART_MODE_TX_RX;
        huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
        huart1.Init.OverSampling = UART_OVERSAMPLING_16;
        huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
        huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
        if (HAL_UART_Init(&huart1) != HAL_OK)
        {
          while(1);
        }

}


// Do not modify these lines of code. They are written to supress UART related warnings
int _read(int file, char *ptr, int len) { return 0; }
int _fstat(int file, struct stat *st) { return 0; }
int _lseek(int file, int ptr, int dir) { return 0; }
int _isatty(int file) { return 1; }
int _close(int file) { return -1; }
int _getpid(void) { return 1; }
int _kill(int pid, int sig) { return -1; }
