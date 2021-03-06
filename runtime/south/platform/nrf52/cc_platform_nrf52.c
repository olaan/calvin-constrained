/*
 * Copyright (c) 2016 Ericsson AB
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include "app_timer.h"
#include "boards.h"
#include "sdk_config.h"
#include "nordic_common.h"
#include "ble_advdata.h"
#include "ble_srv_common.h"
#include "ble_ipsp.h"
#include "ble_6lowpan.h"
#include "mem_manager.h"
#include "nrf_platform_port.h"
#include "app_util_platform.h"
#include "iot_timer.h"
#include "ipv6_medium.h"
#include "nrf_drv_gpiote.h"
#ifdef CC_TLS_ENABLED
#include "nrf_drv_rng.h"
#endif
#include "../cc_platform.h"
#include "../../../north/cc_node.h"
#include "../../../north/cc_transport.h"
#include "../../transport/lwip/cc_transport_lwip.h"
#include "../../../../calvinsys/cc_calvinsys.h"

#define APP_TIMER_OP_QUEUE_SIZE							5
#define LWIP_SYS_TIMER_INTERVAL							APP_TIMER_TICKS(100, APP_TIMER_PRESCALER)
#define APP_CALVIN_CONNECT_DELAY						APP_TIMER_TICKS(5000, APP_TIMER_PRESCALER) // Delay connect as connect fails directly after interface up

APP_TIMER_DEF(m_lwip_timer_id);
APP_TIMER_DEF(m_calvin_connect_timer_id);
eui64_t                                     eui64_local_iid;
static ipv6_medium_instance_t               m_ipv6_medium;

void platform_nrf_app_error_handler(uint32_t error_code, uint32_t line_num, const uint8_t *p_file_name)
{
	cc_log_error("Error 0x%08lX, Line %ld, File %s", error_code, line_num, p_file_name);
//	NVIC_SystemReset();
	for (;;) {
	}
}

void assert_nrf_callback(uint16_t line_num, const uint8_t *p_file_name)
{
	platform_nrf_app_error_handler(0xDEADBEEF, line_num, p_file_name);
}

static void platform_nrf_connectable_mode_enter(void)
{
	uint32_t err_code = ipv6_medium_connectable_mode_enter(m_ipv6_medium.ipv6_medium_instance_id);

	APP_ERROR_CHECK(err_code);

	cc_log_debug("Physical layer in connectable mode");
}

static void platform_nrf_on_ipv6_medium_evt(ipv6_medium_evt_t *p_ipv6_medium_evt)
{
	size_t len = 0;
	transport_client_t *transport_client = transport_lwip_get_client();
	transport_lwip_client_t *transport_state = (transport_lwip_client_t *)transport_client->client_state;

	switch (p_ipv6_medium_evt->ipv6_medium_evt_id) {
	case IPV6_MEDIUM_EVT_CONN_UP:
	{
		cc_log("Physical layer connected mac '%s'", p_ipv6_medium_evt->mac);
		strncpy(transport_state->mac, p_ipv6_medium_evt->mac, strlen(p_ipv6_medium_evt->mac));
		break;
	}
	case IPV6_MEDIUM_EVT_CONN_DOWN:
	{
		cc_log("Physical layer disconnected");
		platform_nrf_connectable_mode_enter();
		transport_client->state = TRANSPORT_INTERFACE_DOWN;
		break;
	}
	default:
	{
		break;
	}
	}
}

static void platform_nrf_on_ipv6_medium_error(ipv6_medium_error_t *p_ipv6_medium_error)
{
	cc_log_error("Physical layer error");
}

static void platform_nrf_ip_stack_init(void)
{
	uint32_t err_code;
	static ipv6_medium_init_params_t ipv6_medium_init_params;
	eui48_t ipv6_medium_eui48;

	APP_TIMER_INIT(APP_TIMER_PRESCALER, APP_TIMER_OP_QUEUE_SIZE, NULL);

	memset(&ipv6_medium_init_params, 0x00, sizeof(ipv6_medium_init_params));
	ipv6_medium_init_params.ipv6_medium_evt_handler    = platform_nrf_on_ipv6_medium_evt;
	ipv6_medium_init_params.ipv6_medium_error_handler  = platform_nrf_on_ipv6_medium_error;
	ipv6_medium_init_params.use_scheduler              = false;

	err_code = ipv6_medium_init(&ipv6_medium_init_params, IPV6_MEDIUM_ID_BLE, &m_ipv6_medium);
	APP_ERROR_CHECK(err_code);

	err_code = ipv6_medium_eui48_get(m_ipv6_medium.ipv6_medium_instance_id, &ipv6_medium_eui48);

	ipv6_medium_eui48.identifier[EUI_48_SIZE - 1] = 0x00;

	err_code = ipv6_medium_eui48_set(m_ipv6_medium.ipv6_medium_instance_id, &ipv6_medium_eui48);
	APP_ERROR_CHECK(err_code);

	err_code = ipv6_medium_eui64_get(m_ipv6_medium.ipv6_medium_instance_id, &eui64_local_iid);
	APP_ERROR_CHECK(err_code);

	err_code = nrf_mem_init();
	APP_ERROR_CHECK(err_code);

	lwip_init();
}

static void platform_nrf_lwip_timer_callback(void *p_ctx)
{
	(void) p_ctx;
	sys_check_timeouts();
}

static void platform_nrf_start_calvin_connect_timer(void)
{
	uint32_t err_code;

	err_code = app_timer_start(m_calvin_connect_timer_id, APP_CALVIN_CONNECT_DELAY, NULL);
	APP_ERROR_CHECK(err_code);
}

static void platform_nrf_calvin_connect_callback(void *p_context)
{
	UNUSED_VARIABLE(p_context);
	transport_client_t *transport_client = transport_lwip_get_client();

	transport_client->state = TRANSPORT_INTERFACE_UP;
}


static void platform_nrf_timers_init(void)
{
	uint32_t err_code;

	// Create and start lwip timer
	err_code = app_timer_create(&m_lwip_timer_id, APP_TIMER_MODE_REPEATED, platform_nrf_lwip_timer_callback);
	APP_ERROR_CHECK(err_code);
	err_code = app_timer_start(m_lwip_timer_id, LWIP_SYS_TIMER_INTERVAL, NULL);
	APP_ERROR_CHECK(err_code);

	// Create calvin init timer used to start the node when a connection is made
	err_code = app_timer_create(&m_calvin_connect_timer_id,
		APP_TIMER_MODE_SINGLE_SHOT,
		platform_nrf_calvin_connect_callback);
	APP_ERROR_CHECK(err_code);
}

void nrf_driver_interface_up(void)
{
	cc_log("IP interface up");
	platform_nrf_start_calvin_connect_timer();
}

void nrf_driver_interface_down(void)
{
	transport_client_t *transport_client = transport_lwip_get_client();

	transport_client->state = TRANSPORT_INTERFACE_DOWN;
	cc_log("IP interface down");
}

result_t platform_stop(node_t *node)
{
	return CC_RESULT_SUCCESS;
}

result_t platform_node_started(struct node_t *node)
{
	return CC_RESULT_SUCCESS;
}

void platform_print(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	fprintf(stderr, "\r\n");
	va_end(args);
}

result_t platform_create(node_t *node)
{
	return list_add_n(&node->proxy_uris, "lwip", 4, NULL, 0);
}

static bool platform_temp_can_read(struct calvinsys_obj_t *obj)
{
	return true;
}

static result_t platform_temp_read(struct calvinsys_obj_t *obj, char **data, size_t *size)
{
	double temp;
	uint32_t err_code;
	int32_t value;

	err_code = sd_temp_get(&value);

	temp = value / 4;

	*size = mp_sizeof_double(temp);
	if (platform_mem_alloc((void **)data, *size) != CC_RESULT_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_RESULT_FAIL;
	}

	mp_encode_double(*data, temp);

	return CC_RESULT_SUCCESS;
}

static calvinsys_obj_t *platform_temp_open(calvinsys_handler_t *handler, char *data, size_t len, unsigned int id)
{
	calvinsys_obj_t *obj = NULL;

	if (platform_mem_alloc((void **)&obj, sizeof(calvinsys_obj_t)) != CC_RESULT_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return NULL;
	}

	obj->write = NULL;
	obj->can_read = platform_temp_can_read;
	obj->read = platform_temp_read;
	obj->close = NULL;
	obj->handler = handler;
	obj->next = NULL;
	handler->objects = obj; // assume only one object

	return obj;
}

result_t platform_create_calvinsys(calvinsys_t **calvinsys)
{
	calvinsys_handler_t *handler = NULL;

	if (platform_mem_alloc((void **)&handler, sizeof(calvinsys_handler_t)) != CC_RESULT_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_RESULT_FAIL;
	}

	handler->open = platform_temp_open;
	handler->objects = NULL;
	handler->next = NULL;

	calvinsys_add_handler(calvinsys, handler);
	if (calvinsys_register_capability(*calvinsys, "io.temperature", handler, NULL) != CC_RESULT_SUCCESS)
		return CC_RESULT_FAIL;

	return CC_RESULT_SUCCESS;
}

void platform_init(void)
{
	uint32_t err_code;
	uint8_t rnd_seed = 0;

	app_trace_init();
	platform_nrf_ip_stack_init();
	err_code = nrf_driver_init();
	APP_ERROR_CHECK(err_code);
	platform_nrf_timers_init();

	platform_nrf_connectable_mode_enter();

	do {
		err_code = sd_rand_application_vector_get(&rnd_seed, 1);
	} while (err_code == NRF_ERROR_SOC_RAND_NOT_ENOUGH_VALUES);
	srand(rnd_seed);

	cc_log("Platform initialized");
}

bool platform_evt_wait(node_t *node, uint32_t timeout_seconds)
{
	if (sd_app_evt_wait() != ERR_OK)
		cc_log_error("Failed to wait for event");

	if (node != NULL && node->transport_client != NULL && node->transport_client->state == TRANSPORT_ENABLED) {
		if (transport_lwip_has_data(node->transport_client)) {
			if (transport_handle_data(node, node->transport_client, node_handle_message) != CC_RESULT_SUCCESS) {
				cc_log_error("Failed to read data from transport");
				node->transport_client->state = TRANSPORT_DISCONNECTED;
				return;
			}
			return true;
		}
	}
	return false;
}

result_t platform_mem_alloc(void **buffer, uint32_t size)
{
	*buffer = malloc(size);
	if (*buffer == NULL) {
		cc_log_error("Failed to allocate '%ld'", (unsigned long)size);
		return CC_RESULT_FAIL;
	}

	return CC_RESULT_SUCCESS;
}

void platform_mem_free(void *buffer)
{
	free(buffer);
}

#ifdef CC_TLS_ENABLED
int platform_random_vector_generate(void *ctx, unsigned char *buffer, size_t size)
{
	uint8_t available;
	uint32_t err_code;
	uint16_t length;

	err_code = nrf_drv_rng_bytes_available(&available);

	cc_log("Requested random numbers 0x%08lx, available 0x%08x",	size,	available);

	if (err_code == NRF_SUCCESS) {
		length = MIN(size, available);
		err_code = nrf_drv_rng_rand(buffer, length);
	}

	return 0;
}
#endif
