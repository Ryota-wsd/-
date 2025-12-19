#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#define ALPH_SIZE 10
#define STR_LEN   15
#define QGRAM     3
#define MAX_G     1000

static inline int ch2code(char c) {
    if (c < 'A' || c > 'J') return -1;
    return c - 'A';
}

static inline int qgram_id(const char *s, int i) {
    int a = ch2code(s[i]), b = ch2code(s[i+1]), c = ch2code(s[i+2]);
    if (a < 0 || b < 0 || c < 0) return -1;
    return a * 100 + b * 10 + c;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "[Error] Usage: %s <db_file>\n", argv[0]);
        return 1;
    }

#ifdef _WIN32
    // Force binary mode on stdout for Windows/PowerShell redirection
    if (_setmode(_fileno(stdout), _O_BINARY) == -1) {
        fprintf(stderr, "[Error] Cannot set binary mode for stdout\n");
        return 1;
    }
#endif

    FILE *fp = fopen(argv[1], "r");
    if (!fp) {
        fprintf(stderr, "[Error] Failed to open DB file: %s\n", argv[1]);
        return 1;
    }

    int N = 0;
    char buf[256];
    while (fgets(buf, sizeof(buf), fp)) {
        buf[strcspn(buf, "\r\n")] = '\0';
        if (strlen(buf) == STR_LEN) N++;
    }

    char (*db)[STR_LEN + 1] = malloc((size_t)N * (STR_LEN + 1));
    if (!db) {
        fprintf(stderr, "[Error] Memory allocation failed for DB array\n");
        fclose(fp);
        return 1;
    }

    rewind(fp);
    int idx = 0;
    while (idx < N && fgets(buf, sizeof(buf), fp)) {
        buf[strcspn(buf, "\r\n")] = '\0';
        if (strlen(buf) == STR_LEN) {
            memcpy(db[idx], buf, STR_LEN);
            db[idx][STR_LEN] = '\0';
            idx++;
        }
    }
    fclose(fp);

    int counts[MAX_G] = {0};
    for (int i = 0; i < N; i++) {
        for (int p = 0; p <= STR_LEN - QGRAM; p++) {
            int g = qgram_id(db[i], p);
            if (g >= 0) counts[g]++;
        }
    }

    int *offset = malloc((MAX_G + 1) * sizeof(int));
    int sum = 0;
    for (int g = 0; g < MAX_G; g++) {
        offset[g] = sum;
        sum += counts[g];
    }
    offset[MAX_G] = sum;
    int total_posts = sum;

    int *postings = malloc((size_t)total_posts * sizeof(int));
    int tmp_counts[MAX_G];
    memcpy(tmp_counts, offset, MAX_G * sizeof(int));
    for (int id = 0; id < N; id++) {
        for (int p = 0; p <= STR_LEN - QGRAM; p++) {
            int g = qgram_id(db[id], p);
            if (g >= 0) postings[tmp_counts[g]++] = id;
        }
    }

    // Binary write to stdout
    if (fwrite(&N, sizeof(int), 1, stdout) != 1) {
        fprintf(stderr, "[Error] Write N failed\n");
    }
    fwrite(db, STR_LEN + 1, (size_t)N, stdout);
    fwrite(offset, sizeof(int), MAX_G + 1, stdout);
    fwrite(&total_posts, sizeof(int), 1, stdout);
    fwrite(postings, sizeof(int), (size_t)total_posts, stdout);

    free(db); free(offset); free(postings);
    return 0;
}
