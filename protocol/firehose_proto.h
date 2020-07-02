#pragma once

#define xml_prefix "<?xml>\n<data>\n"
#define xml_suffix "</data>\n"

gchar *new_firehose_erase(gchar *line);
gchar *new_firehose_program(gchar *line);
gchar *new_firehose_power(gchar *cmd);