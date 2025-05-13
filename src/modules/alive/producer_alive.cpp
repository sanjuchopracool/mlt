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

    float toAliveFps(float frame) const
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
        int error = 0;
        auto pos = mlt_frame_original_position(frame);
        if (mlt_properties_get(properties(), "eof")
                && !::strcmp("loop", mlt_properties_get(properties(), "eof"))) {
            pos %= duration() - 1;
        }
        auto bg = mlt_properties_get_color(properties(), "background");
        alive_color alive_bg{bg.r, bg.g, bg.b, bg.a};
        pos += toMltFps(firstFrame());
        auto image = alive_composition_draw(m_alive_internal,
                                            toAliveFps(pos),
                                            *width, *height,
                                            alive_bg);

        *format = mlt_image_rgba;
        int size = mlt_image_format_size(*format, *width, *height, NULL);
        *buffer = static_cast<uint8_t *>(mlt_pool_alloc(size));
        memcpy(*buffer, image, size);
        error = mlt_frame_set_image(frame, *buffer, size, mlt_pool_release);

        return error;
    }

    bool open(const char *fileName)
    {
        try {
            m_alive_internal = alive_open_composition(fileName, m_error);
            if (m_alive_internal) {
                return true;
            } else {
                print_error();
                return false;
            }
        }
        catch(...) {
            std::cout << "Exception thrown while opening " << fileName
                      << ".Make sure it has the correct version." << std::endl;
        }
        return false;
    }

    void print_error() {
        std::cout << m_error <<std::endl;
    }
};
extern "C" {

// Forward declarations
static int producer_alive_get_frame(mlt_producer producer, mlt_frame_ptr frame, int index);
static void producer_alive_close(mlt_producer producer);

/** Initialize the producer.
 */
mlt_producer producer_alive_init(mlt_profile profile, mlt_service_type type, const char *id, char *arg)
{
    // Allocate the producer
    Alive *alive = new Alive();
    mlt_producer producer = (mlt_producer) calloc(1, sizeof(*producer));

    if (!alive || mlt_producer_init(producer, alive)) {
        mlt_producer_close(producer);
        return NULL;
    }
    // If allocated and initializes
    if (alive->open(arg)) {
        alive->setProducer(producer);
        alive->m_profile = profile;
        producer->close = (mlt_destructor) producer_alive_close;
        producer->get_frame = producer_alive_get_frame;

        auto properties = alive->properties();
        mlt_properties_set(properties, "resource", arg);
        mlt_properties_set(properties, "background", "#00000000");
        mlt_properties_set_int(properties, "aspect_ratio", 1);
        mlt_properties_set_int(properties, "progressive", 1);
        mlt_properties_set_int(properties, "seekable", 1);
        mlt_properties_set_int(properties, "meta.media.width", alive->width());
        mlt_properties_set_int(properties, "meta.media.height", alive->height());
        mlt_properties_set_int(properties, "meta.media.sample_aspect_num", 1);
        mlt_properties_set_int(properties, "meta.media.sample_aspect_den", 1);
        mlt_properties_set_double(properties, "meta.media.frame_rate", alive->fps());
        mlt_properties_set_int(properties, "out", alive->duration() - 1);
        mlt_properties_set_int(properties, "length", alive->duration());
        mlt_properties_set_int(properties, "first_frame", alive->firstFrame());
        mlt_properties_set(properties, "eof", "loop");
    }
    return producer;
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
static int produce_alive_get_image(mlt_frame frame, uint8_t **buffer, mlt_image_format *format, int *width, int *height, int writable)
{
    auto producer = static_cast<mlt_producer>(mlt_frame_pop_service(frame));
    auto alive = static_cast<Alive *>(producer->child);

    if (mlt_properties_get_int(alive->properties(), "refresh")) {
        mlt_properties_clear(alive->properties(), "refresh");
        alive->open(mlt_properties_get(alive->properties(), "resource"));
        if (alive->duration() > mlt_properties_get_int(alive->properties(), "length")) {
            mlt_properties_set_int(alive->properties(), "length", alive->duration());
        }
    }

    return alive->getImage(frame, buffer, format, width, height, writable);
}

/** Get the next frame.
 *
 * @param producer a producer
 * @param frame a frame by reference
 * @param index the frame number
 * @return 0 if successful, otherwise non-zero
 */
static int producer_alive_get_frame(mlt_producer producer, mlt_frame_ptr frame, int index)
{
    *frame = mlt_frame_init(MLT_PRODUCER_SERVICE(producer));
    mlt_properties frame_properties = MLT_FRAME_PROPERTIES(*frame);

    // Set frame properties
    mlt_properties_set_int(frame_properties, "progressive", 1);
    // Inform framework that this producer creates rgba frames by default
    mlt_properties_set_int(frame_properties, "format", mlt_image_rgba);
    double force_ratio = mlt_properties_get_double(MLT_PRODUCER_PROPERTIES(producer),
                                                   "force_aspect_ratio");
    if (force_ratio > 0.0)
        mlt_properties_set_double(frame_properties, "aspect_ratio", force_ratio);
    else
        mlt_properties_set_double(frame_properties, "aspect_ratio", 1.0);

    mlt_frame_set_position(*frame, mlt_producer_position(producer));
    mlt_frame_push_service(*frame, producer);
    mlt_frame_push_get_image(*frame, produce_alive_get_image);
    mlt_producer_prepare_next(producer);
    return 0;
}

/** Close the producer.
 */
static void producer_alive_close(mlt_producer producer)
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
    auto init_alive_lib = []{
        char error[1024];
        alive_init("/home/sanju/inae_resources", error, log_level::trace);
        std::cout << error << std::endl;
    };
    std::call_once(g_init_alive_library, init_alive_lib);

    // Register our alive producer
    MLT_REGISTER(mlt_service_producer_type, "alive", producer_alive_init);
    // Register metadata
    MLT_REGISTER_METADATA(mlt_service_producer_type, "alive",  metadata, NULL);
}

} // extern "C"
