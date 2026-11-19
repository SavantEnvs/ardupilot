/*
 * A minimal "vehicle" for the parameter/mission harnesses: a top-level AP_Param table holding one
 * scalar of each parameter type plus the mission, fence and rally objects, so the parameter
 * parsers and the mission/fence/rally stores have real objects to find by name and write into.
 */
#pragma once

#define AP_PARAM_VEHICLE_NAME fuzz_vehicle

#include <AP_Param/AP_Param.h>
#include <AP_Mission/AP_Mission.h>
#include <AC_Fence/AC_Fence.h>
#include <AP_Rally/AP_Rally.h>
#include <AP_InertialSensor/AP_InertialSensor.h>
#include <AP_Baro/AP_Baro.h>
#include <AP_GPS/AP_GPS.h>
#include <AP_Compass/AP_Compass.h>
#include <AP_AHRS/AP_AHRS.h>
#include <AP_Logger/AP_Logger.h>
#include <GCS_MAVLink/GCS_Dummy.h>

class Parameters {
public:
    enum {
        k_param_format_version = 0,
        k_param_i8,
        k_param_i16,
        k_param_i32,
        k_param_flt,
        k_param_vec,
        k_param_mission,
        k_param_fence,
        k_param_rally,
    };
    AP_Int16 format_version;
    AP_Int8 i8;
    AP_Int16 i16;
    AP_Int32 i32;
    AP_Float flt;
    AP_Vector3f vec;
};

class FuzzVehicle {
public:
    Parameters g;
    // the mission/rally code takes home from AP::ahrs(), which needs its sensors to exist
    AP_InertialSensor ins;
    AP_Baro baro;
    AP_GPS gps;
    Compass compass;
    AP_AHRS ahrs{};
    // rally/fence changes are logged through AP::logger() (no backend is started)
    AP_Logger logger;
    AP_Mission mission{
        FUNCTOR_BIND_MEMBER(&FuzzVehicle::start_cmd, bool, const AP_Mission::Mission_Command &),
        FUNCTOR_BIND_MEMBER(&FuzzVehicle::verify_cmd, bool, const AP_Mission::Mission_Command &),
        FUNCTOR_BIND_MEMBER(&FuzzVehicle::mission_complete, void)};
    AC_Fence fence;
    AP_Rally rally;
    GCS_Dummy gcs;

    static const AP_Param::Info var_info[];
    AP_Param param_loader{var_info};

    // load the parameter table (this is where a `--defaults` file is applied) and the stores
    void init()
    {
        AP_Param::check_var_info();
        const bool ok_setup = AP_Param::setup();
        AP_Param::setup_sketch_defaults();
        const bool ok_load = AP_Param::load_all();
        ::printf("params: setup=%d load=%d\n", ok_setup, ok_load);
        mission.init();
        fence.init();
    }

private:
    bool start_cmd(const AP_Mission::Mission_Command &) { return true; }
    bool verify_cmd(const AP_Mission::Mission_Command &) { return true; }
    void mission_complete() {}
};

static FuzzVehicle fuzz_vehicle;

const AP_Param::Info FuzzVehicle::var_info[] = {
    GSCALAR(format_version, "FORMAT_VERSION", 0),
    GSCALAR(i8,  "FZ_I8",  1),
    GSCALAR(i16, "FZ_I16", 2),
    GSCALAR(i32, "FZ_I32", 3),
    GSCALAR(flt, "FZ_FLT", 4.5f),
    GSCALAR(vec, "FZ_VEC", 0),
    GOBJECT(mission, "MIS_",   AP_Mission),
    GOBJECT(fence,   "FENCE_", AC_Fence),
    GOBJECT(rally,   "RALLY_", AP_Rally),
    AP_VAREND
};
