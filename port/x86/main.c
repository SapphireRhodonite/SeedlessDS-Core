#include <stdio.h>
#include <string.h>

#include "seedlessds/nds.h"
#include "seedlessds/platform.h"
#include "seedlessds/cpu_backend.h"
#include "seedlessds/script.h"

const nds_platform_t *port_platform(void);

int main(int argc, char **argv)
{
    const cpu_backend_t *backend = nds_cpu_backend_interp();
    if (!backend) {
        fprintf(stderr, "no interp backend\n");
        return 1;
    }
    printf("backend: %s\n", backend->name ? backend->name : "(unnamed)");
    printf("version: %s\n", nds_version_string());

    nds_t *nds = nds_create(port_platform(), 0, 0);
    if (!nds) {
        fprintf(stderr, "nds_create returned NULL\n");
        return 1;
    }

    if (argc > 1) {
        nds_rom_open_t open_args;
        memset(&open_args, 0, sizeof open_args);
        open_args.path = argv[1];
        open_args.state_slot = -1;
        if (!nds_load_rom(nds, &open_args)) {
            fprintf(stderr, "nds_load_rom failed\n");
            nds_quit(nds);
            nds_destroy(nds);
            return 1;
        }
        nds_input_t input;
        memset(&input, 0, sizeof input);
        for (int f = 0; f < 3; f++) nds_run_frame(nds, &input);
        printf("frame: %u\n", nds_frame_word(nds));
    }

    nds_quit(nds);
    nds_destroy(nds);
    return 0;
}
