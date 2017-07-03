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
#ifndef ACTOR_SOIL_HUMIDITY_H
#define ACTOR_SOIL_HUMIDITY_H

#include "../runtime/north/cc_actor.h"
#include "../calvinsys/cc_calvinsys.h"

result_t actor_soil_moisture_init(actor_t **actor, list_t *attributes);
result_t actor_soil_moisture_set_state(actor_t **actor, list_t *attributes);
bool actor_soil_moisture_fire(actor_t *actor);
void actor_soil_moisture_free(actor_t *actor);

#endif /* ACTOR_SOIL_HUMIDITY_H */