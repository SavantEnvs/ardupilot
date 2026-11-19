/*
 * Shared driver for the file-input ArduPilot harnesses (AFL targets, `@@` on the command line).
 *
 * main() takes the input path as the LAST argument and hides it from the SITL HAL's own option
 * parser, then runs the HAL. The HAL's setup() callback reads the file and hands its bytes to the
 * harness's fuzz_one(); the process then exits, so every input runs in a freshly started HAL.
 * The HAL keeps its storage in ./eeprom.bin; setup() erases it first (as Tools/Replay does), so
 * nothing an earlier input stored can change how this input runs.
 */
#pragma once

#include <AP_HAL/AP_HAL.h>
#include <AP_Param/AP_Param.h>
#include <StorageManager/StorageManager.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>

extern const AP_HAL::HAL& hal;

static void fuzz_one(const std::vector<uint8_t> &data);

static const char *fuzz_input_path;

static bool fuzz_read_file(const char *path, std::vector<uint8_t> &out)
{
    FILE *f = ::fopen(path, "rb");
    if (f == nullptr) {
        return false;
    }
    uint8_t buf[4096];
    size_t n;
    while ((n = ::fread(buf, 1, sizeof(buf), f)) > 0) {
        out.insert(out.end(), buf, buf + n);
    }
    ::fclose(f);
    return true;
}

static void fuzz_setup()
{
    std::vector<uint8_t> data;
    if (!fuzz_read_file(fuzz_input_path, data)) {
        ::printf("cannot read %s\n", fuzz_input_path);
        exit(1);
    }
    StorageManager::erase();
    AP_Param::erase_all();
    fuzz_one(data);
    exit(0);
}

static void fuzz_loop() {}

static AP_HAL::HAL::FunCallbacks fuzz_callbacks(fuzz_setup, fuzz_loop);

int main(int argc, char * const argv[]);
int main(int argc, char * const argv[])
{
    if (argc < 2) {
        ::printf("usage: %s <input file>\n", argv[0]);
        return 1;
    }
    fuzz_input_path = argv[argc - 1];
    // SITL ignores the command line for example-style programs; only argv[0] is passed on
    hal.run(1, argv, &fuzz_callbacks);
    return 0;
}
