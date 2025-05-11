/**
 * factory.c - the factory method interfaces
 * Copyright (C) 2025 Your Name
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <framework/mlt.h>
#include <string.h>

extern mlt_producer producer_alive_init(mlt_profile profile, mlt_service_type type, const char *id, char *arg);

static mlt_properties metadata(mlt_service_type type, const char *id, void *data)
{
    if (type != mlt_service_producer_type) {
        return NULL;
    }
    return mlt_properties_parse_yaml("producer_alive.yml");
}

MLT_REPOSITORY
{
    // Register our alive producer
    MLT_REGISTER(mlt_service_producer_type, "alive", producer_alive_init);
    // Register metadata
    MLT_REGISTER_METADATA(mlt_service_producer_type, "alive",  metadata, NULL);
}
