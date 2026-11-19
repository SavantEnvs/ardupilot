/*
 * MAVLink FTP uploads of the virtual files a ground station writes to replace the whole
 * mission, fence, rally list or parameter set: AP::FS() open / write / close on
 *   @MISSION/mission.dat, @MISSION/fence.dat, @MISSION/rally.dat, @PARAM/param.pck
 * close() parses the uploaded image (AP_Filesystem_Mission::finish_upload ->
 * AP_Mission::mavlink_int_to_mission_cmd / fence / rally, AP_Filesystem_Param::param_upload_parse).
 *
 * Input: byte 0 selects the file (low 2 bits); the rest is the file content, written in
 * 239-byte chunks like MAVFTP's WriteFile data.
 */
#include "fuzz_common.h"
#include "fuzz_vehicle.h"
#include <AP_Filesystem/AP_Filesystem.h>
#include <fcntl.h>

const AP_HAL::HAL& hal = AP_HAL::get_HAL();

static void fuzz_one(const std::vector<uint8_t> &d)
{
    if (d.empty()) {
        return;
    }
    fuzz_vehicle.init();

    static const char *files[4] = {
        "@MISSION/mission.dat", "@MISSION/fence.dat", "@MISSION/rally.dat", "@PARAM/param.pck",
    };
    const char *fname = files[d[0] & 3];
    int fd = AP::FS().open(fname, O_WRONLY | O_CREAT | O_TRUNC);
    if (fd < 0) {
        ::printf("open %s failed\n", fname);
        return;
    }
    size_t ofs = 1;
    while (ofs < d.size()) {
        const size_t n = MIN(d.size() - ofs, (size_t)239);
        if (AP::FS().write(fd, &d[ofs], n) != (int32_t)n) {
            break;
        }
        ofs += n;
    }
    const int ret = AP::FS().close(fd);
    ::printf("%s upload: %s; mission items=%u FZ_I8=%d\n", fname, ret == 0 ? "accepted" : "rejected",
             (unsigned)fuzz_vehicle.mission.num_commands(), (int)fuzz_vehicle.g.i8.get());
}
