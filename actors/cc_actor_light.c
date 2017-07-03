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
#include <stdlib.h>
#include <string.h>
#include "cc_actor_light.h"
#include "../runtime/north/cc_msgpack_helper.h"
#include "../runtime/north/cc_fifo.h"
#include "../msgpuck/msgpuck.h"

result_t actor_light_init(actor_t **actor, list_t *attributes)
{
	calvinsys_obj_t *obj = NULL;

	obj = calvinsys_open((*actor)->calvinsys, "io.light", NULL, 0);
	if (obj == NULL) {
		cc_log_error("Failed to open 'io.light'");
		return CC_RESULT_FAIL;
	}

	(*actor)->instance_state = (void *)obj;

	return CC_RESULT_SUCCESS;
}

result_t actor_light_set_state(actor_t **actor, list_t *attributes)
{
	return actor_light_init(actor, attributes);
}

bool actor_light_fire(struct actor_t *actor)
{
	port_t *inport = (port_t *)actor->in_ports->data;
	calvinsys_obj_t *obj = (calvinsys_obj_t *)actor->instance_state;
	token_t *token = NULL;

	if (fifo_tokens_available(inport->fifo, 1)) {
		token = fifo_peek(inport->fifo);
		if (obj->write(obj, token->value, token->size) == CC_RESULT_SUCCESS) {
			fifo_commit_read(inport->fifo, true);
			return true;
		}
		fifo_cancel_commit(inport->fifo);
	}

	return false;
}

void actor_light_free(actor_t *actor)
{
	calvinsys_close((calvinsys_obj_t *)actor->instance_state);
}