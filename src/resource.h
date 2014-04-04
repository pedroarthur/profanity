/*
 * resource.h
 *
 * Copyright (C) 2012 - 2014 James Booth <boothj5@gmail.com>
 *
 * This file is part of Profanity.
 *
 * Profanity is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Profanity is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Profanity.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef RESOURCE_H
#define RESOURCE_H

#include "common.h"

typedef struct resource_t {
    char *name;
    resource_presence_t presence;
    char *status;
    int priority;
    char *caps_str;
} Resource;

Resource * resource_new(const char * const name, resource_presence_t presence,
    const char * const status, const int priority, const char * const caps_str);
void resource_destroy(Resource *resource);

int resource_compare_availability(Resource *first, Resource *second);

#endif
