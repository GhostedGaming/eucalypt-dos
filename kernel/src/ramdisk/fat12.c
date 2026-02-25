#include <ramdisk/fat12.h>
#include <ramdisk/ramdisk.h>
#include <x86_64/allocator/heap.h>
#include <string.h>

typedef struct {
    uint8_t  jmp_boot[3];
    uint8_t  oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sector_count;
    uint8_t  num_fats;
    uint16_t root_entry_count;
    uint16_t total_sectors_16;
    uint8_t  media;
    uint16_t fat_size_16;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    uint8_t  drive_number;
    uint8_t  reserved1;
    uint8_t  boot_sig;
    uint32_t volume_id;
    uint8_t  volume_label[11];
    uint8_t  file_system_type[8];
    uint8_t  boot_code[448];
    uint16_t boot_sector_signature;
} __attribute__((packed)) bpb_t;

typedef struct {
    bpb_t   *bpb;
    uint8_t *fat;
    uint32_t fat_start_sector;
    uint32_t root_dir_start_sector;
    uint32_t data_start_sector;
    uint32_t root_dir_sectors;
} fat12_t;

static fat12_t          *g_fat12 = NULL;
static file_descriptor_t fd_table[MAX_FDS];

static void format_name(const char *filename, uint8_t *name_out, uint8_t *ext_out) {
    int k = 0;
    while (filename[k] && filename[k] != '.' && k < 8) {
        name_out[k] = filename[k];
        k++;
    }
    while (k < 8) name_out[k++] = ' ';

    const char *ext = filename;
    while (*ext && *ext != '.') ext++;
    if (*ext == '.') ext++;

    k = 0;
    while (ext[k] && k < 3) {
        ext_out[k] = ext[k];
        k++;
    }
    while (k < 3) ext_out[k++] = ' ';
}

static void parse_name(dir_entry_t *entry, char *out) {
    int k = 0;
    for (int i = 0; i < 8 && entry->name[i] != ' '; i++) out[k++] = entry->name[i];
    if (entry->ext[0] != ' ') {
        out[k++] = '.';
        for (int i = 0; i < 3 && entry->ext[i] != ' '; i++) out[k++] = entry->ext[i];
    }
    out[k] = '\0';
}

static bool entry_name_matches(dir_entry_t *entry, dir_entry_t *other) {
    for (int i = 0; i < 8; i++) if (entry->name[i] != other->name[i]) return false;
    for (int i = 0; i < 3; i++) if (entry->ext[i]  != other->ext[i])  return false;
    return true;
}

uint16_t read_fat_entry(uint16_t cluster) {
    uint32_t fat_offset   = cluster + (cluster / 2);
    uint32_t fat_sector   = g_fat12->fat_start_sector + (fat_offset / g_fat12->bpb->bytes_per_sector);
    uint32_t entry_offset = fat_offset % g_fat12->bpb->bytes_per_sector;

    uint8_t *buffer = kmalloc(g_fat12->bpb->bytes_per_sector);
    read_ramdisk_sector(fat_sector, buffer);

    uint16_t value = (cluster & 1)
        ? (*(uint16_t *)&buffer[entry_offset]) >> 4
        : (*(uint16_t *)&buffer[entry_offset]) & 0x0FFF;

    kfree(buffer);
    return value;
}

void write_fat_entry(uint16_t cluster, uint16_t value) {
    uint32_t fat_offset   = cluster + (cluster / 2);
    uint32_t fat_sector   = g_fat12->fat_start_sector + (fat_offset / g_fat12->bpb->bytes_per_sector);
    uint32_t entry_offset = fat_offset % g_fat12->bpb->bytes_per_sector;

    uint8_t *buffer = kmalloc(g_fat12->bpb->bytes_per_sector);
    read_ramdisk_sector(fat_sector, buffer);

    if (cluster & 1) {
        uint16_t current = *(uint16_t *)&buffer[entry_offset];
        *(uint16_t *)&buffer[entry_offset] = (current & 0x000F) | (value << 4);
    } else {
        uint16_t current = *(uint16_t *)&buffer[entry_offset];
        *(uint16_t *)&buffer[entry_offset] = (current & 0xF000) | (value & 0x0FFF);
    }

    for (int i = 0; i < g_fat12->bpb->num_fats; i++) {
        uint32_t sector = fat_sector + (g_fat12->bpb->fat_size_16 * i);
        write_ramdisk_sector(sector, buffer);
    }

    kfree(buffer);
}

uint16_t allocate_cluster() {
    uint16_t max = (g_fat12->bpb->fat_size_16 * g_fat12->bpb->bytes_per_sector * 2) / 3;
    for (uint16_t cluster = 2; cluster < max; cluster++) {
        if (read_fat_entry(cluster) == 0) {
            write_fat_entry(cluster, 0xFFF);
            return cluster;
        }
    }
    return 0;
}

void free_cluster_chain(uint16_t start_cluster) {
    uint16_t cluster = start_cluster;
    while (cluster >= 2 && cluster < 0xFF8) {
        uint16_t next = read_fat_entry(cluster);
        write_fat_entry(cluster, 0);
        cluster = next;
    }
}

uint32_t cluster_to_sector(uint16_t cluster) {
    return g_fat12->data_start_sector + ((cluster - 2) * g_fat12->bpb->sectors_per_cluster);
}

dir_entry_t *find_file(const char *filename) {
    uint8_t *buffer = kmalloc(g_fat12->bpb->bytes_per_sector);

    for (uint32_t i = 0; i < g_fat12->root_dir_sectors; i++) {
        read_ramdisk_sector(g_fat12->root_dir_start_sector + i, buffer);

        uint32_t entries_per_sector = g_fat12->bpb->bytes_per_sector / sizeof(dir_entry_t);
        for (uint32_t j = 0; j < entries_per_sector; j++) {
            dir_entry_t *entry = (dir_entry_t *)(buffer + j * sizeof(dir_entry_t));

            if (entry->name[0] == 0x00) { kfree(buffer); return NULL; }
            if (entry->name[0] == 0xE5) continue;

            char name[12];
            parse_name(entry, name);

            if (strcmp(name, filename) == 0) {
                dir_entry_t *result = kmalloc(sizeof(dir_entry_t));
                memcpy(result, entry, sizeof(dir_entry_t));
                kfree(buffer);
                return result;
            }
        }
    }

    kfree(buffer);
    return NULL;
}

dir_entry_t *create_file(const char *filename) {
    uint8_t *buffer = kmalloc(g_fat12->bpb->bytes_per_sector);

    for (uint32_t i = 0; i < g_fat12->root_dir_sectors; i++) {
        read_ramdisk_sector(g_fat12->root_dir_start_sector + i, buffer);

        uint32_t entries_per_sector = g_fat12->bpb->bytes_per_sector / sizeof(dir_entry_t);
        for (uint32_t j = 0; j < entries_per_sector; j++) {
            dir_entry_t *entry = (dir_entry_t *)(buffer + j * sizeof(dir_entry_t));

            if (entry->name[0] == 0x00 || entry->name[0] == 0xE5) {
                memset(entry, 0, sizeof(dir_entry_t));
                format_name(filename, entry->name, entry->ext);
                entry->attr              = 0x20;
                entry->first_cluster_low = 0;
                entry->file_size         = 0;

                write_ramdisk_sector(g_fat12->root_dir_start_sector + i, buffer);

                dir_entry_t *result = kmalloc(sizeof(dir_entry_t));
                memcpy(result, entry, sizeof(dir_entry_t));
                kfree(buffer);
                return result;
            }
        }
    }

    kfree(buffer);
    return NULL;
}

uint8_t *read_file(dir_entry_t *file, uint32_t *size) {
    if (!file || file->file_size == 0) { *size = 0; return NULL; }

    *size = file->file_size;
    uint8_t *data      = kmalloc(file->file_size);
    uint32_t bytes_read = 0;
    uint16_t cluster    = file->first_cluster_low;
    uint32_t cluster_size = g_fat12->bpb->bytes_per_sector * g_fat12->bpb->sectors_per_cluster;

    while (cluster >= 2 && cluster < 0xFF8 && bytes_read < file->file_size) {
        uint32_t sector  = cluster_to_sector(cluster);
        uint8_t *buffer  = kmalloc(cluster_size);

        for (int i = 0; i < g_fat12->bpb->sectors_per_cluster; i++)
            read_ramdisk_sector(sector + i, buffer + i * g_fat12->bpb->bytes_per_sector);

        uint32_t remaining = file->file_size - bytes_read;
        uint32_t to_copy   = remaining < cluster_size ? remaining : cluster_size;
        memcpy(data + bytes_read, buffer, to_copy);
        bytes_read += to_copy;

        kfree(buffer);
        cluster = read_fat_entry(cluster);
    }

    return data;
}

void write_file(const char *filename, uint8_t *data, uint32_t size) {
    dir_entry_t *file = find_file(filename);

    if (!file) {
        file = create_file(filename);
    } else if (file->first_cluster_low) {
        free_cluster_chain(file->first_cluster_low);
    }

    if (!file) return;

    file->file_size = size;

    uint32_t bytes_written = 0;
    uint16_t prev_cluster  = 0;
    uint16_t first_cluster = 0;
    uint32_t cluster_size  = g_fat12->bpb->bytes_per_sector * g_fat12->bpb->sectors_per_cluster;

    while (bytes_written < size) {
        uint16_t cluster = allocate_cluster();
        if (cluster == 0) break;

        if (first_cluster == 0) first_cluster = cluster;
        if (prev_cluster   != 0) write_fat_entry(prev_cluster, cluster);

        uint32_t remaining = size - bytes_written;
        uint32_t to_write  = remaining < cluster_size ? remaining : cluster_size;
        uint32_t sector    = cluster_to_sector(cluster);

        uint8_t *buffer = kmalloc(cluster_size);
        memset(buffer, 0, cluster_size);
        memcpy(buffer, data + bytes_written, to_write);

        for (int i = 0; i < g_fat12->bpb->sectors_per_cluster; i++)
            write_ramdisk_sector(sector + i, buffer + i * g_fat12->bpb->bytes_per_sector);

        kfree(buffer);
        bytes_written += to_write;
        prev_cluster   = cluster;
    }

    file->first_cluster_low = first_cluster;

    uint8_t *root_buffer = kmalloc(g_fat12->bpb->bytes_per_sector);
    uint32_t entries_per_sector = g_fat12->bpb->bytes_per_sector / sizeof(dir_entry_t);

    for (uint32_t i = 0; i < g_fat12->root_dir_sectors; i++) {
        read_ramdisk_sector(g_fat12->root_dir_start_sector + i, root_buffer);

        for (uint32_t j = 0; j < entries_per_sector; j++) {
            dir_entry_t *entry = (dir_entry_t *)(root_buffer + j * sizeof(dir_entry_t));
            if (entry_name_matches(entry, file)) {
                memcpy(entry, file, sizeof(dir_entry_t));
                write_ramdisk_sector(g_fat12->root_dir_start_sector + i, root_buffer);
                kfree(root_buffer);
                kfree(file);
                return;
            }
        }
    }

    kfree(root_buffer);
    kfree(file);
}

int32_t open_file(const char *filename) {
    int32_t fd = -1;
    for (int32_t i = 0; i < MAX_FDS; i++) {
        if (!fd_table[i].open) { fd = i; break; }
    }
    if (fd == -1) return -1;

    dir_entry_t *entry = find_file(filename);
    if (!entry) return -2;

    uint32_t size = 0;
    uint8_t *data = read_file(entry, &size);
    if (!data) { kfree(entry); return -3; }

    fd_table[fd] = (file_descriptor_t){
        .entry    = entry,
        .data     = data,
        .size     = size,
        .position = 0,
        .open     = true,
    };

    return fd;
}

void close_file(int32_t fd) {
    if (fd < 0 || fd >= MAX_FDS || !fd_table[fd].open) return;
    kfree(fd_table[fd].entry);
    kfree(fd_table[fd].data);
    fd_table[fd].open = false;
}

int32_t read_file_fd(int32_t fd, uint8_t *buf, uint32_t len) {
    if (fd < 0 || fd >= MAX_FDS || !fd_table[fd].open || !buf || len == 0) return -1;

    file_descriptor_t *f  = &fd_table[fd];
    uint32_t remaining    = f->size - f->position;
    if (remaining == 0) return 0;

    uint32_t to_read = len < remaining ? len : remaining;
    memcpy(buf, f->data + f->position, to_read);
    f->position += to_read;

    return (int32_t)to_read;
}

int32_t seek_file(int32_t fd, uint32_t position) {
    if (fd < 0 || fd >= MAX_FDS || !fd_table[fd].open) return -1;
    if (position > fd_table[fd].size) return -2;
    fd_table[fd].position = position;
    return 0;
}

uint32_t tell_file(int32_t fd) {
    if (fd < 0 || fd >= MAX_FDS || !fd_table[fd].open) return 0;
    return fd_table[fd].position;
}

void init_fat12() {
    g_fat12       = kmalloc(sizeof(fat12_t));
    g_fat12->bpb  = kmalloc(sizeof(bpb_t));

    read_ramdisk_sector(0, (uint8_t *)g_fat12->bpb);

    g_fat12->fat_start_sector     = g_fat12->bpb->reserved_sector_count;
    g_fat12->root_dir_sectors     = ((g_fat12->bpb->root_entry_count * 32) + (g_fat12->bpb->bytes_per_sector - 1)) / g_fat12->bpb->bytes_per_sector;
    g_fat12->root_dir_start_sector = g_fat12->fat_start_sector + (g_fat12->bpb->num_fats * g_fat12->bpb->fat_size_16);
    g_fat12->data_start_sector    = g_fat12->root_dir_start_sector + g_fat12->root_dir_sectors;

    for (int i = 0; i < MAX_FDS; i++) fd_table[i].open = false;
}