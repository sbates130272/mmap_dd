////////////////////////////////////////////////////////////////////////
//
// Copyright 2014 PMC-Sierra, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License"); you
// may not use this file except in compliance with the License. You may
// obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0 Unless required by
// applicable law or agreed to in writing, software distributed under the
// License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for
// the specific language governing permissions and limitations under the
// License.
//
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
//
//   Author: Logan Gunthorpe
//
//   Date:   Oct 23 2014
//
//   Description:
//     Simple utility that functions similar to dd but uses mmap to
//     read/write data from a file.
//
////////////////////////////////////////////////////////////////////////

#include <argconfig/argconfig.h>
#include <argconfig/report.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/time.h>

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

const char program_desc[] =
    "data dump and write using mmap calls ";

struct config {
    long count;
    long skip;
    int write_mode;
    int mem_mode;
    int block_size;
};

static const struct config defaults = {
    .count = 128,
    .block_size = 1,
};

static const struct argconfig_commandline_options command_line_options[] = {
    {"b",             "block_size", CFG_POSITIVE, &defaults.block_size, required_argument,
            "number of bytes to read/write at once"
    },
    {"n",             "length", CFG_LONG_SUFFIX, &defaults.count, required_argument,
            "bytes to read/write"
    },
    {"m",            "", CFG_NONE, &defaults.mem_mode, no_argument,
            "read data into memory, don't output to stdout"},
    {"s",             "skip", CFG_LONG_SUFFIX, &defaults.skip, required_argument,
            "skip bytes from the input file"
    },
    {"w",               "", CFG_NONE, &defaults.write_mode, no_argument,
            "write data from stdin"},
    {0}
};

int main(int argc, char **argv)
{
    struct config cfg;

    argconfig_append_usage("file");

    int args = argconfig_parse(argc, argv, program_desc, command_line_options,
                               &defaults, &cfg, sizeof(cfg));
    argv[args+1] = NULL;

    if (args != 1)  {
        argconfig_print_help(argv[0], program_desc, command_line_options);
        return -1;
    }

    int fd = open(argv[1], cfg.write_mode ? O_RDWR : O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "mmap_dd: %s: %s\n", argv[1], strerror(errno));
        return -1;
    }

    void *addr = mmap(NULL, cfg.count,
                      (cfg.write_mode ? PROT_WRITE : 0) | PROT_READ,
                      MAP_SHARED, fd, cfg.skip);
    if (addr == MAP_FAILED) {
        perror("mmap failed");
        return -1;
    }

    cfg.count /= cfg.block_size;

    struct timeval start_time;
    gettimeofday(&start_time, NULL);


    size_t transfered;
    if (cfg.mem_mode) {
        void *buf = malloc(cfg.count * cfg.block_size);
        memcpy(buf, addr, cfg.count * cfg.block_size);
        transfered = cfg.count;
        free(buf);
    } else if (cfg.write_mode) {
        transfered = fread(addr, cfg.block_size, cfg.count, stdin);
    } else {
        transfered = fwrite(addr, cfg.block_size, cfg.count, stdout);
    }

    transfered *= cfg.block_size;

    struct timeval end_time;
    gettimeofday(&end_time, NULL);

    munmap(addr, cfg.count * cfg.block_size);

    fprintf(stderr, "\nCopied ");
    report_transfer_rate(stderr, &start_time, &end_time, transfered);
    fprintf(stderr, "\n");

    return 0;
}
