Firehose Support
================

Introduction
------------

This plugin is used to update hardware that uses the firehose protocol.

Firmware Format
---------------

The daemon will decompress the cabinet archive and extract a firmware blob in
ZIP file format. Inside the zip file must be all the firmware images for each
partition and a rawprogram*.xml file. The partition images can be in any format, but
the rawprogram*.xml should be `rawprogram.xml` format file.

All partitions with a defined image found in the zip file will be updated.

This plugin supports the following protocol ID:

 * com.qualcomm.firehose
 * com.qualcomm.sahara

GUID Generation
---------------

These devices use the standard USB DeviceInstanceId values, e.g.

 * `USB\VID_05C6&PID_9008`
