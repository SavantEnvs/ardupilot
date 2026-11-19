/*
 * Parameter defaults files (SITL `--defaults <file>`, a board's defaults.parm):
 * AP_Param::load_defaults_file() on the input, the call AP_Param::load_all() makes for the HAL's
 * defaults file (AP_Filesystem fgets, parse_param_line, find_by_name, set_value), over a parameter
 * table with one scalar of each type plus the mission, fence and rally groups.
 */
#include "fuzz_common.h"
#include "fuzz_vehicle.h"

const AP_HAL::HAL& hal = AP_HAL::get_HAL();

static void fuzz_one(const std::vector<uint8_t> &)
{
    fuzz_vehicle.init();
    const bool ok = AP_Param::load_defaults_file(fuzz_input_path, true);
    ::printf("defaults file: %s\n", ok ? "loaded" : "rejected");
    const Vector3f v = fuzz_vehicle.g.vec.get();
    ::printf("FZ_I8=%d FZ_I16=%d FZ_I32=%d FZ_FLT=%f FZ_VEC=%f,%f,%f MIS_TOTAL=%u\n",
             (int)fuzz_vehicle.g.i8.get(), (int)fuzz_vehicle.g.i16.get(), (int)fuzz_vehicle.g.i32.get(),
             (double)fuzz_vehicle.g.flt.get(), (double)v.x, (double)v.y, (double)v.z,
             (unsigned)fuzz_vehicle.mission.num_commands_max());
}
