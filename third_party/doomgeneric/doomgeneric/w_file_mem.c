// WAD odwzorowany w pamieci: platforma dostarcza wskaznik (host: mmap pliku, ESP32: esp_partition_mmap).
// Dzieki temu W_CacheLumpNum zwraca wskaznik wprost do danych (bez kopiowania do strefy).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "doomtype.h"
#include "w_file.h"
#include "z_zone.h"

// implementowane przez platforme; zwraca NULL jesli sie nie da
extern const unsigned char *DG_MapWad(const char *path, unsigned int *length);

extern wad_file_class_t mem_wad_file;

static wad_file_t *W_Mem_OpenFile(char *path)
{
    unsigned int len = 0;
    const unsigned char *base = DG_MapWad(path, &len);
    if (base == NULL)
        return NULL;

    wad_file_t *result = malloc(sizeof(wad_file_t));
    result->file_class = &mem_wad_file;
    result->mapped = (byte *) base;
    result->length = len;
    return result;
}

static void W_Mem_CloseFile(wad_file_t *wad)
{
    free(wad);
}

// Normalnie nieuzywane (mapped != NULL), ale zaimplementowane dla kompletnosci.
static size_t W_Mem_Read(wad_file_t *wad, unsigned int offset, void *buffer, size_t buffer_len)
{
    if (offset >= wad->length)
        return 0;
    if (offset + buffer_len > wad->length)
        buffer_len = wad->length - offset;
    memcpy(buffer, wad->mapped + offset, buffer_len);
    return buffer_len;
}

wad_file_class_t mem_wad_file =
{
    W_Mem_OpenFile,
    W_Mem_CloseFile,
    W_Mem_Read,
};
