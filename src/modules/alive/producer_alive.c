/**
 * producer_alive.c - alive producer for MLT Framework
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
#include <stdlib.h>
#include <string.h>

// Forward declarations
static int producer_get_frame(mlt_producer producer, mlt_frame_ptr frame, int index);
static void producer_close(mlt_producer producer);

/** Initialize the producer.
 */
mlt_producer producer_alive_init(mlt_profile profile, mlt_service_type type, const char *id, char *arg)
{
    // Create a producer
    mlt_producer producer = mlt_producer_new(profile);

    // Initialize if successful
    if (producer) {
        // Get the properties
        mlt_properties properties = MLT_PRODUCER_PROPERTIES(producer);

        // Set callbacks
        producer->get_frame = producer_get_frame;
        producer->close = (mlt_destructor)producer_close;

        // Set default properties
        mlt_properties_set(properties, "resource", arg ? arg : "alive_resource");

        // Set default video properties
        mlt_properties_set_int(properties, "width", profile->width);
        mlt_properties_set_int(properties, "height", profile->height);
        mlt_properties_set_double(properties, "aspect_ratio", mlt_profile_sar(profile));
        mlt_properties_set_int(properties, "progressive", 1);
        // mlt_properties_set_double(properties, "fps", profile->fps);

        // Set length to 100 frames by default
        mlt_properties_set_position(properties, "length", 100);
        mlt_properties_set_position(properties, "out", 99);

        // Register the producer for clean-up
        mlt_properties_set_data(properties, "_producer", producer, 0, NULL, NULL);

        return producer;
    }

    return NULL;
}
/** Generate the image.
 *
 * @param frame a frame object
 * @param format the format of the image (returned)
 * @param width the horizontal size in pixels (returned)
 * @param height the vertical size in pixels (returned)
 * @param writable whether the returned buffer can be written to
 * @return a pointer to the image data
 */
static int get_image(mlt_frame frame, uint8_t **image, mlt_image_format *format, int *width, int *height, int writable)
{
    // Get the properties of the frame
    mlt_properties properties = MLT_FRAME_PROPERTIES(frame);

    // Get the requested dimensions
    int requested_width = *width;
    int requested_height = *height;

    // Use the requested or the default values
    *width = requested_width > 0 ? requested_width : mlt_properties_get_int(properties, "meta.media.width");
    *height = requested_height > 0 ? requested_height : mlt_properties_get_int(properties, "meta.media.height");

    // Make sure we have valid dimensions
    if (*width <= 0)
        *width = 720;
    if (*height <= 0)
        *height = 576;

    // Determine image format
    if (*format == mlt_image_none || *format == mlt_image_movit)
        *format = mlt_image_rgba;

    // Calculate image size based on format
    int size = mlt_image_format_size(*format, *width, *height, NULL);

    // Allocate the image
    *image = mlt_pool_alloc(size);
    if (!*image)
        return 1;

    // Update the frame properties
    mlt_frame_set_image(frame, *image, size, mlt_pool_release);

    // Get the color values stored in the frame properties
    int red = mlt_properties_get_int(properties, "alive_red");
    int green = mlt_properties_get_int(properties, "alive_green");
    int blue = mlt_properties_get_int(properties, "alive_blue");

    // Fill the image buffer with our color
    if (*format == mlt_image_rgba) {
        uint8_t *p = *image;
        int i, count = *width * *height;

        for (i = 0; i < count; i++) {
            *p++ = red;    // R
            *p++ = green;  // G
            *p++ = blue;   // B
            *p++ = 255;    // A (fully opaque)
        }
    } else {
        // Handle other formats if needed
        // (for simplicity, we're just handling RGBA format here)
        memset(*image, 128, size);
    }

    return 0;
}

/** Get the next frame.
 *
 * @param producer a producer
 * @param frame a frame by reference
 * @param index the frame number
 * @return 0 if successful, otherwise non-zero
 */
static int producer_get_frame(mlt_producer producer, mlt_frame_ptr frame, int index)
{
    // Create an empty frame
    *frame = mlt_frame_init(MLT_PRODUCER_SERVICE(producer));

    if (*frame) {
        // Get the producer properties
        mlt_properties producer_props = MLT_PRODUCER_PROPERTIES(producer);

        // Get the frame properties
        mlt_properties frame_props = MLT_FRAME_PROPERTIES(*frame);

        // Set frame properties
        mlt_properties_set_int(frame_props, "progressive",
                               mlt_properties_get_int(producer_props, "progressive"));
        mlt_properties_set_double(frame_props, "aspect_ratio",
                                  mlt_properties_get_double(producer_props, "aspect_ratio"));

        // Get the current position
        mlt_position position = mlt_producer_position(producer);

        // Set the position on the frame
        mlt_frame_set_position(*frame, position);

        // Calculate color values based on position
        uint8_t red = (position * 2) % 255;
        uint8_t green = (position * 3) % 255;
        uint8_t blue = (position * 5) % 255;

        // Store these color values for use in get_image callbacks
        mlt_properties_set_int(frame_props, "alive_red", red);
        mlt_properties_set_int(frame_props, "alive_green", green);
        mlt_properties_set_int(frame_props, "alive_blue", blue);

        // Setup callbacks
        mlt_frame_push_get_image(*frame, get_image);

        // Update timecode on the frame
        mlt_properties_set_int(frame_props, "meta.media.width",
                               mlt_properties_get_int(producer_props, "width"));
        mlt_properties_set_int(frame_props, "meta.media.height",
                               mlt_properties_get_int(producer_props, "height"));

        // Advance the producer
        mlt_producer_prepare_next(producer);

        return 0;
    }

    return 1;
}

/** Close the producer.
 */
static void producer_close(mlt_producer producer)
{
    // Close the parent
    producer->close = NULL;
    mlt_producer_close(producer);

    // Free the producer
    free(producer);
}
