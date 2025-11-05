#pragma once

#include "IotaWatt.h"
#include "Uploader.h"
#include "Emoncms_uploader.h"
#include "influxDB_uploader.h"
#include "influxDB2_uploader.h"
#include "PVoutput_uploader.h"

void declare_uploaders();
Uploader *new_uploader(const char *ID);
bool delete_uploader(const char *ID);
char **get_uploader_list();
int get_uploader_count();
Uploader *get_uploader_instance(const char *ID);