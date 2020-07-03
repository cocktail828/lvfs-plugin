#include "firehose_proto.h"
#include "../fu-quectel-common.h"

#include <stdio.h>

static char firehose_command_buffer[1024];

char *new_firehose_program(firehose_program *program)
{
    snprintf(firehose_command_buffer, array_len(firehose_command_buffer),
               "<?xml version=\"1.0\" ?>\n"
               "<data>\n "
               "<program verbose=\"%s\" pages_per_block=\"%d\" sector_size_in_bytes=\"%d\" filename=\"%s\" label=\"%s\" "
               "last_sector=\"%d\" num_partition_sectors=\"%d\" physical_partition_number=\"%d\" start_sector=\"%d\"/>"
               "</data>",
               program->verbose ? "true" : "false",
               program->pages_per_block,
               program->sector_size_in_bytes,
               program->filename,
               program->label,
               program->last_sector,
               program->num_partition_sectors,
               program->physical_partition_number,
               program->start_sector);
    return (char *)firehose_command_buffer;
}

char *new_firehose_erase(firehose_erase *erase)
{
    snprintf(firehose_command_buffer, array_len(firehose_command_buffer),
               "<?xml version=\"1.0\" ?>\n"
               "<data>\n "
               "<erase verbose=\"%s\" pages_per_block=\"%d\" sector_size_in_bytes=\"%d\" label=\"%s\" "
               "last_sector=\"%d\" num_partition_sectors=\"%d\" physical_partition_number=\"%d\" start_sector=\"%d\"/>"
               "</data>",
               erase->verbose ? "true" : "false",
               erase->pages_per_block,
               erase->sector_size_in_bytes,
               erase->label,
               erase->last_sector,
               erase->num_partition_sectors,
               erase->physical_partition_number,
               erase->start_sector);
    return (char *)firehose_command_buffer;
}

char *new_firehose_power(char *cmd)
{
    snprintf(firehose_command_buffer, array_len(firehose_command_buffer),
               "<?xml version=\"1.0\" ?>\n"
               "<data>\n"
               "<power value=\"%s\" />"
               "</data>",
               cmd);
    return (char *)firehose_command_buffer;
}

char *new_firehose_configure(int max_tx_size)
{
    snprintf(firehose_command_buffer, array_len(firehose_command_buffer),
               "<?xml version=\"1.0\" ?>\n"
               "<data>\n"
               "<configure verbose=\"true\" MaxPayloadSizeFromTargetInBytes=\"%d\" "
               "MaxPayloadSizeToTargetInBytes=\"%d\""
               "/>"
               "</data>",
               2048, max_tx_size);
    return (char *)firehose_command_buffer;
}