#pragma once

#include <stdint.h>

typedef struct
{
    int verbose;
    int pages_per_block;
    int sector_size_in_bytes;
    int last_sector;
    int num_partition_sectors;
    int physical_partition_number;
    int start_sector;
    char label[0];
} firehose_erase;

typedef struct
{
    int verbose;
    int pages_per_block;
    int sector_size_in_bytes;
    int last_sector;
    int num_partition_sectors;
    int physical_partition_number;
    int start_sector;
    char label[64];
    char filename[0];
} firehose_program;

char *new_firehose_program(firehose_program *program);
char *new_firehose_erase(firehose_erase *erase);

/**
 * <power> attributes Description
 * reset_to_edl       Reset to emergency download mode
 * reset              Reset and try booting normally
 * off                Poweroff and do not turn back on
 * Delayinseconds/    Number of seconds to delay before executing power command;
 *                    typically used for powering off the device because the USB cable must be removed first
 */
char *new_firehose_power(char *cmd);

char *new_firehose_configure(int max_tx_size);