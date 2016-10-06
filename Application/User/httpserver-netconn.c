/**
 ******************************************************************************
 * @file    LwIP/LwIP_HTTP_Server_Netconn_RTOS/Src/httpser-netconn.c
 * @author  MCD Application Team
 * @version V1.3.5
 * @date    03-June-2016
 * @brief   Basic http server implementation using LwIP netconn API
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; Copyright � 2016 STMicroelectronics International N.V.
 * All rights reserved.</center></h2>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted, provided that the following conditions are met:
 *
 * 1. Redistribution of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of STMicroelectronics nor the names of other
 *    contributors to this software may be used to endorse or promote products
 *    derived from this software without specific written permission.
 * 4. This software, including modifications and/or derivative works of this
 *    software, must execute solely and exclusively on microcontroller or
 *    microprocessor devices manufactured by or for STMicroelectronics.
 * 5. Redistribution and use of this software other than as permitted under
 *    this license is void and will automatically terminate your rights under
 *    this license.
 *
 * THIS SOFTWARE IS PROVIDED BY STMICROELECTRONICS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS, IMPLIED OR STATUTORY WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NON-INFRINGEMENT OF THIRD PARTY INTELLECTUAL PROPERTY
 * RIGHTS ARE DISCLAIMED TO THE FULLEST EXTENT PERMITTED BY LAW. IN NO EVENT
 * SHALL STMICROELECTRONICS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ******************************************************************************
 */
/* Includes ------------------------------------------------------------------*/
#include "lwip/opt.h"
#include "lwip/arch.h"
#include "lwip/api.h"
#include "fs.h"
#include "string.h"
#include "httpserver-netconn.h"
#include "cmsis_os.h"
#include "stm32f4_discovery.h"

#include <stdio.h>

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
#define WEBSERVER_THREAD_PRIO    ( tskIDLE_PRIORITY + 4 )

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
u32_t nPageHits = 0;

/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/**
 * @brief serve tcp connection
 * @param conn: pointer on connection structure
 * @retval None
 */
static void http_server_serve(struct netconn *conn) {
	struct netbuf *inbuf;
	err_t recv_err;
	char* buf;
	u16_t buflen;
	struct fs_file * file;

	/* Read the data from the port, blocking if nothing yet there.
	 We assume the request (the part we care about) is in one netbuf */
	recv_err = netconn_recv(conn, &inbuf);

	if (recv_err == ERR_OK) {
		if (netconn_err(conn) == ERR_OK) {
			netbuf_data(inbuf, (void**) &buf, &buflen);

			/* Is this an HTTP GET command? (only check the first 5 chars, since
			 there are other formats for GET, and we're keeping it very simple )*/
			if ((buflen >= 5) && (strncmp(buf, "GET /", 5) == 0)) {
				/* Check if request to get ST.gif */
				if (strncmp((char const *) buf, "GET /img/logoFI.png", 19) == 0) {
					file = fs_open("/img/logoFI.png");
					netconn_write(conn, (const unsigned char* )(file->data),(size_t )file->len, NETCONN_NOCOPY);
					fs_close(file);
				} else if (strncmp(buf, "GET /pure.css", 13) == 0) {
					file = fs_open("/pure.css");
					netconn_write(conn, (const unsigned char* )(file->data),(size_t )file->len, NETCONN_NOCOPY);
					fs_close(file);
				} else if (strncmp(buf, "GET /nanoajax.min.js", 20) == 0) {
					file = fs_open("/nanoajax.min.js");
					netconn_write(conn, (const unsigned char* )(file->data),(size_t )file->len, NETCONN_NOCOPY);
					fs_close(file);
				} else if (strncmp(buf, "GET /tasks.html", 15) == 0) {
					/* Load dynamic page */
					ReturnTasks(conn);
				} else if (strncmp(buf, "GET /leds.html", 14) == 0) {
					/* Load dynamic page */
					ReturnLeds(conn,buf);
				} else if (strncmp(buf, "GET /ADC.html", 13) == 0) {
					/* Load dynamic page */
					ReturnADC(conn);
				} else if ((strncmp(buf, "GET /index.html", 15) == 0)
						|| (strncmp(buf, "GET / ", 6) == 0)) {
					/* Load Index page */
					file = fs_open("/index.html");
					netconn_write(conn, (const unsigned char* )(file->data),(size_t )file->len, NETCONN_NOCOPY);
					fs_close(file);
				} else {
					/* Load Error page */
					file = fs_open("/404.html");
					netconn_write(conn, (const unsigned char* )(file->data),(size_t )file->len, NETCONN_NOCOPY);
					fs_close(file);
				}
			}
		}
	}
	/* Close the connection (server closes in HTTP) */
	netconn_close(conn);

	/* Delete the buffer (netconn_recv gives us ownership,
	 so we have to make sure to deallocate the buffer) */
	netbuf_delete(inbuf);
}

/**
 * @brief  http server thread
 * @param arg: pointer on argument(not used here)
 * @retval None
 */
static void http_server_netconn_thread(void *arg) {
	struct netconn *conn, *newconn;
	err_t err, accept_err;

	/* Create a new TCP connection handle */
	conn = netconn_new(NETCONN_TCP);

	if (conn != NULL) {
		/* Bind to port 80 (HTTP) with default IP address */
		err = netconn_bind(conn, NULL, 80);

		if (err == ERR_OK) {
			/* Put the connection into LISTEN state */
			netconn_listen(conn);

			while (1) {
				/* accept any icoming connection */
				accept_err = netconn_accept(conn, &newconn);
				if (accept_err == ERR_OK) {
					/* serve connection */
					http_server_serve(newconn);

					/* delete connection */
					netconn_delete(newconn);
				}
			}
		}
	}
}

/**
 * @brief  Initialize the HTTP server (start its thread)
 * @param  none
 * @retval None
 */
void http_server_netconn_init() {
	sys_thread_new("HTTP", http_server_netconn_thread, NULL,
			1024, WEBSERVER_THREAD_PRIO);
}


void ReturnTasks(struct netconn *conn) {
	portCHAR PAGE_BODY[512];
	memset(PAGE_BODY, 0, 512);

	strcat((char *) PAGE_BODY,	"HTTP/1.0 200 OK\n\n");
	strcat((char *) PAGE_BODY,	"<pre><br>Name          State  Priority  Stack   Num");
	strcat((char *) PAGE_BODY,	"<br>---------------------------------------------<br>");

	/* The list of tasks and their status */
	osThreadList((unsigned char *) (PAGE_BODY + strlen(PAGE_BODY)));
	strcat((char *) PAGE_BODY,	"<br><br>---------------------------------------------");
	strcat((char *) PAGE_BODY,	"<br>B : Blocked, R : Ready, D : Deleted, S : Suspended<br>");

	/* Send the dynamically generated page */

	netconn_write(conn, PAGE_BODY, strlen(PAGE_BODY), NETCONN_COPY);
}


void ReturnADC(struct netconn *conn) {
	portCHAR PAGE_BODY[512];
	memset(PAGE_BODY, 0, 512);

	sprintf(PAGE_BODY,"HTTP/1.0 200 OK\n\n%d",BSP_ADC_GetValue()/41);

	/* Send the dynamically generated page */

	netconn_write(conn, PAGE_BODY, strlen(PAGE_BODY), NETCONN_COPY);
}

void ReturnLeds(struct netconn *conn, char* buf) {
	portCHAR PAGE_BODY[512];
	memset(PAGE_BODY, 0, 512);

	char *led1, *led2, *led3, *led4;

	// GET /leds.html?l1=1 HTTP/1.0
	// GET /leds.html?l2=0 HTTP/1.0

	led1 = strstr(buf,"l1=");
	led2 = strstr(buf,"l2=");
	led3 = strstr(buf,"l3=");
	led4 = strstr(buf,"l4=");

	if(led1 != NULL){
		//tengo el led1
		if(led1[3] == '0')
			BSP_LED_Off(BARRA1);
		else
			BSP_LED_On(BARRA1);
	}
	if(led2 != NULL){
		//tengo el led1
		if(led2[3] == '0')
			BSP_LED_Off(BARRA2);
		else
			BSP_LED_On(BARRA2);
	}
	if(led3 != NULL){
		//tengo el led1
		if(led3[3] == '0')
			BSP_LED_Off(BARRA3);
		else
			BSP_LED_On(BARRA3);
	}
	if(led4 != NULL){
		//tengo el led1
		if(led4[3] == '0')
			BSP_LED_Off(BARRA4);
		else
			BSP_LED_On(BARRA4);
	}



	strcat((char *) PAGE_BODY,	"HTTP/1.0 200 OK\nContent-Type: application/json\n\n"
			"{\"leds\": [");

	if(BSP_LED_Read(BARRA1))
		strcat((char *) PAGE_BODY,"1,");
	else
		strcat((char *) PAGE_BODY,"0,");


	if(BSP_LED_Read(BARRA2))
		strcat((char *) PAGE_BODY,"1,");
	else
		strcat((char *) PAGE_BODY,"0,");


	if(BSP_LED_Read(BARRA3))
		strcat((char *) PAGE_BODY,"1,");
	else
		strcat((char *) PAGE_BODY,"0,");


	if(BSP_LED_Read(BARRA4))
		strcat((char *) PAGE_BODY,"1");
	else
		strcat((char *) PAGE_BODY,"0");


	strcat((char *) PAGE_BODY,"]}\n");

	netconn_write(conn, PAGE_BODY, strlen(PAGE_BODY), NETCONN_COPY);
}
