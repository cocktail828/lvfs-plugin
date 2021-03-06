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

/**
 * After receiving the <program> command, the target responds by indicating it is now in raw data mode
 * via <response rawmode="true". The host must then begin sending as many raw data packets as
 * indicated by num_partition_sectors. 
 * For example, if MaxPayloadSizeToTargetInBytes=131072 (128 KB), then with
 * num_partition_sectors=4096 and SECTOR_SIZE_IN_BYTES=512, this is 2 MB of total data,
 * which would take 16 raw data packets of size 128 KB. After this, the target responds with an ACK or
 * NAK indicating if the write was successful.
 * Once the target is in rawmode="true", it treats any and all bytes received on the COM port as raw
 * data. If a <program> command was sent with num_partition_sectors="200" and
 * SECTOR_SIZE_IN_BYTES="512", the next 100 KB the target receives (200*512 bytes/sector) is
 * assumed to be raw data meant for the write. Even if an XML file was sent to the target (i.e., a
 * command), it treats the bytes of that XML file as raw data meant to be written to disk as instructed by
 * the previous <program> command.
 * The rawprogram.xml files are autogenerated by the partitioning tool, ptool.py, and are an XML
 * representation of a partition table. It is possible that a particular partition could have
 * num_partition_sectors="4194304", which is 2 GB, but the file needed there to program is only
 * 10 sectors (5 KB). In this case, it is recommended to modify the num_partition_sectors="10"
 * before sending the <program> command, because this is all the actual file data to program.
 * Conversely, if leaving it as num_partition_sectors="4194304", it tells the target that much data
 * will be sent. Therefore, it would require sending 10 sectors of actual file data, and then zero pad out
 * the remaining 4194294 (1.99 GB), which would be time-consuming and unnecessary.
 * The physical_partition_number attribute is used to determine the LUN number when
 * programming a UFS device.
 */
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

/**
 * The <configure> command retrieves and shares information with the target.
 * MaxPayloadSizeToTargetInBytes is an important attribute. In the <configure> command, the
 * host informs the target of the payload size (in bytes) it intends to use. When the target responds, it
 * informs the host of the maximum payload size it can handle. If the target can handle the payload size
 * requested by the host, it echoes this value back.
 */
typedef struct
{
    int verbose;
    int AlwaysValidate;                  /* Causes the validation operation to occur on every packet (VIP is not
                                                    enabled by this). This is used to see the impact of validation (hashing)
                                                    without enabling secure boot.*/
    int MaxDigestTableSizeInBytes;       /* Used with VIP; indicates maximum size of the Digest Table, i.e., 8 KB. */
    int MaxPayloadSizeToTargetInBytes;   /* Maximum raw data payload size, i.e., 128 KB. This value must be a
                                                    multiple of 512. It is highly recommended that this value is not smaller
                                                    than SECTOR_SIZE_IN_BYTES. */
    int MaxPayloadSizeFromTargetInBytes; /* This parameter is only present in a <configure> response from the
                                            target. It represents the maximum raw data payload size that the target sends back to the host.
                                            MaxPayloadSizeToTargetInBytes must be a multiple of 512. For good performance, it is highly
                                            recommended that it is not smaller than SECTOR_SIZE_IN_BYTES (found in the rawprogram.xml
                                            files). Therefore, if SECTOR_SIZE_IN_BYTES is 4096 (4KB), it is recommended that
                                            MaxPayloadSizeToTargetInBytes is set to 4096, 8192, 16384, etc. */
    int ZlpAwareHost;                    /* The host can send this attribute to the device programmer to indicate if
                                                    the host expects a zero length packet to terminate transfers. For
                                                    Windows hosts, this is set to 1; for Linux hosts, this is set to 0. The
                                                    default is 1 for a Windows host. */
    int SkipStorageInit;                 /* This skips calling UFSInit or EMMCInit. This must be defined for UFS
                                                    devices before they have been provisioned, otherwise initialization fails.
                                                    The default is 1, so storage is initialized, if not provided. */
    char MemoryName[0];                  /* This can be ufs or emmc depending on the type of device expecting on
                                                    the device. If not provided, it defaults to emmc. */
} firehose_configure;

/**
 * <power> attributes Description
 * reset_to_edl       Reset to emergency download mode
 * reset              Reset and try booting normally
 * off                Poweroff and do not turn back on
 * Delayinseconds/    Number of seconds to delay before executing power command;
 *                    typically used for powering off the device because the USB cable must be removed first
 */