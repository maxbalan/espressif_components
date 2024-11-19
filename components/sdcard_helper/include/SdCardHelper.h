#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "errno.h"
#ifndef __sdcard_h__
#define __sdcard_h__

typedef struct pin_config {
    int miso;
    int mosi;
    int clk;
    int cs;
} pin_config;

typedef struct {
    pin_config pin_mode;
    int max_req_khz;
    char mount_point[10];
} sdcard_config;

struct SdCard {
    void *data;
    sdmmc_card_t *card;
    sdmmc_host_t host;
    sdcard_config *config;
    bool err;
};

typedef struct SdCard SdCard;

#endif

SdCard sdcard_mount(sdcard_config sdcard_config);

void sdcard_unmount(SdCard *card);

void sdcard_delete_file(SdCard *card, const char *source_file_path);

void sdcard_move_file(SdCard *card, const char *source_file_path, const char *destination_file_path);