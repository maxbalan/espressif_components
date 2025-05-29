#include "SdCardHelper.h"

#ifndef DEBUG_ENABLED
#define DEBUG_ENABLED
#endif

#ifdef DEBUG_ENABLED
static const char *TAG = "SdCard >>> ";
#endif

SdCard sdcard_mount(sdcard_config config) {
    SdCard sd_card;
    esp_err_t ret;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024,
    };

    sdmmc_card_t *card;
    ESP_LOGI(TAG, "Initializing SD card");
    ESP_LOGI(TAG, "Using SPI peripheral");

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.max_freq_khz = config.max_req_khz;
    host.command_timeout_ms = 3000;

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = config.pin_mode.mosi,
        .miso_io_num = config.pin_mode.miso,
        .sclk_io_num = config.pin_mode.clk,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 8192,
    };

    ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize bus.");
        sd_card.err = true;
        return sd_card;
    }

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = config.pin_mode.cs;
    slot_config.host_id = host.slot;

    ESP_LOGI(TAG, "Mounting filesystem");
    ret = esp_vfs_fat_sdspi_mount(config.mount_point, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG,
                     "Failed to mount filesystem. "
                     "If you want the card to be formatted, set the CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.");
        } else {
            ESP_LOGE(TAG,
                     "Failed to initialize the card (%s). "
                     "Make sure SD card lines have pull-up resistors in place.",
                     esp_err_to_name(ret));
        }

        sd_card.err = true;
        return sd_card;
    }
    ESP_LOGI(TAG, "Filesystem mounted");

    sdmmc_card_print_info(stdout, card);

    sd_card.config = &config;
    sd_card.host = host;
    sd_card.card = card;
    sd_card.err = false;

    return sd_card;
}

void sdcard_unmount(SdCard *card) {
    esp_vfs_fat_sdcard_unmount(card->config->mount_point, card->card);
    ESP_LOGI(TAG, "Card unmounted");
    spi_bus_free(card->host.slot);
}

void sdcard_create_dir(const char *path) {
    struct stat st = {0};

    if (stat(path, &st) == -1) {
        mkdir(path, 0755);
    }
}

void sdcard_create_file(SdCard *card, const char *file_path) {
    struct stat st = {0};
    char *dir_path = strdup(file_path);
    char *last_slash = strrchr(dir_path, '/');

    if (last_slash != NULL) {
        *last_slash = '\0';  // null-terminate at last slash to get directory path

        if (stat(dir_path, &st) == -1) {
            sdcard_create_dir(dir_path);
        }
        free(dir_path);

        FILE *file = fopen(file_path, "w");
        if (file) {
            fclose(file);
            ESP_LOGI(TAG, "File created successfully: %s\n", file_path);
        } else {
            ESP_LOGE(TAG, "Error: Couldn't create file %s\n", file_path);
        }
    } else {
        ESP_LOGE(TAG, "Error: No directory separator found in the path.\n");
        free(dir_path);
    }
}

void sdcard_delete_file(SdCard *card, const char *file_path) {
    struct stat hSt;

    if (stat(file_path, &hSt) == 0) {
        // Delete it if it exists
        unlink(file_path);
        ESP_LOGI(TAG, "File deleted!");
    } else {
        ESP_LOGI(TAG, "File not found or failed to delete!");
    }
}

void sdcard_move_file(SdCard *card, const char *source_file_path, const char *destination_file_path) {
    struct stat st;
    char *oldFilePath = malloc(strlen(source_file_path) + strlen(card->config->mount_point));
    char *newFilePath = malloc(strlen(destination_file_path) + strlen(card->config->mount_point));

    if (!oldFilePath || !newFilePath) {
        ESP_LOGE(TAG, "Error: Memory allocation failed.\n");
        free(oldFilePath);
        free(newFilePath);
        return;
    }

    sprintf(oldFilePath, "%s%s", card->config->mount_point, source_file_path);
    sprintf(newFilePath, "%s%s", card->config->mount_point, destination_file_path);

    if (access(oldFilePath, F_OK) != 0) {
        ESP_LOGE(TAG, "Error: Source file does not exist.\n");
        free(oldFilePath);
        free(newFilePath);
        return;
    }

    ESP_LOGI(TAG, "rename file[ %s ] to [ %s ]", oldFilePath, newFilePath);

    sdcard_delete_file(card, newFilePath);

    // Rename original file
    if (rename(oldFilePath, newFilePath) != 0) {
        ESP_LOGE(TAG, "Rename failed [ %d ]", errno);
        return;
    }

    free(oldFilePath);
    free(newFilePath);
}