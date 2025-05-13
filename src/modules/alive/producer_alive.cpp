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
#include <alive_api.h>
#include <mutex>
#include <iostream>
#include <cmath>

namespace {
std::once_flag g_init_alive_library;
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
    *image = static_cast<uint8_t *>(mlt_pool_alloc(size));
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

class Alive
{
private:
    mlt_producer m_producer = nullptr;
    char m_error[1024];
    void* m_alive_internal = nullptr;;

public:
    mlt_profile m_profile = nullptr;
    ~Alive() {
        if (m_alive_internal) {
            alive_close_composition(m_alive_internal, m_error);
            print_error();
        }
    }

    void setProducer(mlt_producer producer) { m_producer = producer; }

    mlt_producer producer() const { return m_producer; }

    mlt_service service() const { return MLT_PRODUCER_SERVICE(m_producer); }

    mlt_properties properties() const { return MLT_PRODUCER_PROPERTIES(m_producer); }

    int32_t width() const {return alive_compoistion_width(m_alive_internal);}
    int32_t height() const {return alive_compoistion_height(m_alive_internal);}

    int duration() const
    {
        auto frames = alive_composition_last_frame(m_alive_internal)
                      - alive_composition_first_frame(m_alive_internal);
        return toMltFps(frames);
    }

    int toMltFps(float frame) const
    {
        return std::round(frame / fps() * m_profile->frame_rate_num / m_profile->frame_rate_den);
    }

    float toGlaxnimateFps(float frame) const
    {
        return frame * fps() * m_profile->frame_rate_den / m_profile->frame_rate_num;
    }

    int firstFrame() const { return toMltFps(alive_composition_first_frame(m_alive_internal)); }

    float fps() const { return alive_composition_fps(m_alive_internal); }

    int getImage(mlt_frame frame,
                 uint8_t **buffer,
                 mlt_image_format *format,
                 int *width,
                 int *height,
                 int writable)
    {
        return 0;
    }

    bool open(const char *fileName)
    {
        m_alive_internal = alive_open_composition(fileName, m_error);

        if (m_alive_internal) {
            return true;
        } else {
            print_error();
            return false;
        }
    }

    void print_error() {
        std::cout << m_error <<std::endl;
    }
};

// Forward declarations
static int producer_get_frame(mlt_producer producer, mlt_frame_ptr frame, int index);
static void producer_close(mlt_producer producer);

/** Initialize the producer.
 */
mlt_producer producer_alive_init(mlt_profile profile, mlt_service_type type, const char *id, char *arg)
{
    // Allocate the producer
    Alive *glax = new Alive();
    mlt_producer producer = (mlt_producer) calloc(1, sizeof(*producer));

    if (!glax || mlt_producer_init(producer, glax)) {
        mlt_producer_close(producer);
        return NULL;
    }
    // If allocated and initializes
    if (glax->open(arg)) {
        glax->setProducer(producer);
        glax->m_profile = profile;
        producer->close = (mlt_destructor) producer_close;
        producer->get_frame = producer_get_frame;

        auto properties = glax->properties();
        mlt_properties_set(properties, "resource", arg);
        mlt_properties_set(properties, "background", "#00000000");
        mlt_properties_set_int(properties, "aspect_ratio", 1);
        mlt_properties_set_int(properties, "progressive", 1);
        mlt_properties_set_int(properties, "seekable", 1);
        mlt_properties_set_int(properties, "meta.media.width", glax->width());
        mlt_properties_set_int(properties, "meta.media.height", glax->height());
        mlt_properties_set_int(properties, "meta.media.sample_aspect_num", 1);
        mlt_properties_set_int(properties, "meta.media.sample_aspect_den", 1);
        mlt_properties_set_double(properties, "meta.media.frame_rate", glax->fps());
        mlt_properties_set_int(properties, "out", glax->duration() - 1);
        mlt_properties_set_int(properties, "length", glax->duration());
        mlt_properties_set_int(properties, "first_frame", glax->firstFrame());
        mlt_properties_set(properties, "eof", "loop");
    }
    return producer;
}

/** Close the producer.
 */
static void producer_close(mlt_producer producer)
{
    delete static_cast<Alive *>(producer->child);
    producer->close = nullptr;
    mlt_producer_close(producer);
}

static mlt_properties metadata(mlt_service_type type, const char *id, void *data)
{
    if (type != mlt_service_producer_type) {
        return NULL;
    }
    return mlt_properties_parse_yaml("producer_alive.yml");
}

MLT_REPOSITORY
{
    char error[1024];
    std::call_once(g_init_alive_library, alive_init,"home/sanju/inae_resources", error, log_level::trace);
    std::cout << error << std::endl;
    // Register our alive producer
    MLT_REGISTER(mlt_service_producer_type, "alive", producer_alive_init);
    // Register metadata
    MLT_REGISTER_METADATA(mlt_service_producer_type, "alive",  metadata, NULL);
}
