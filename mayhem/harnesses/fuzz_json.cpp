/*
 * AP_JSON (the picojson-derived parser behind SITL frame models, the X-Plane dataref map and the
 * vehicle-info files): AP_JSON::load_json() on the input file, then walk the parsed value the way
 * its consumers (SIM_Frame, SIM_XPlane) read it.
 */
#include "fuzz_common.h"
#include <AP_JSON/AP_JSON.h>
#include <GCS_MAVLink/GCS_Dummy.h>
#include <string>

const AP_HAL::HAL& hal = AP_HAL::get_HAL();

static GCS_Dummy _gcs;

static size_t walk(const AP_JSON::value &v, unsigned depth)
{
    size_t n = 1;
    if (depth > 64) {
        return n;
    }
    if (v.is<AP_JSON::value::object>()) {
        for (const auto &kv : v.get<AP_JSON::value::object>()) {
            if (v.contains(kv.first)) {
                n += walk(v.get(kv.first), depth + 1);
            }
        }
    } else if (v.is<AP_JSON::value::array>()) {
        const auto &a = v.get<AP_JSON::value::array>();
        for (size_t i = 0; i < a.size(); i++) {
            n += walk(v.get(i), depth + 1);
        }
    } else if (v.is<double>()) {
        n += (v.get<double>() > 0);
    } else if (v.is<std::string>()) {
        n += v.get<std::string>().size() & 1;
    } else {
        n += v.evaluate_as_boolean();
    }
    return n;
}

static void fuzz_one(const std::vector<uint8_t> &d)
{
    // load_json() reads the file itself, through AP::FS(), which on SITL only resolves paths
    // below the working directory: stage the input there under a fixed relative name
    const char *staged = "fuzz_input.json";
    FILE *f = ::fopen(staged, "wb");
    if (f == nullptr || (!d.empty() && ::fwrite(d.data(), 1, d.size(), f) != d.size())) {
        ::printf("cannot stage %s\n", staged);
        exit(1);
    }
    ::fclose(f);
    AP_JSON::value *obj = AP_JSON::load_json(staged);
    if (obj == nullptr) {
        ::printf("json: rejected\n");
        return;
    }
    const size_t n = walk(*obj, 0);
    const std::string s = obj->to_str();
    ::printf("json: %u nodes, %u bytes re-serialised\n", (unsigned)n, (unsigned)s.size());
    delete obj;
}
