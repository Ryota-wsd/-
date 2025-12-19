#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

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

// Myers' bit-parallel algorithm
static inline int edit_distance_le3(const char *s, const char *t) {
    uint32_t Peq[ALPH_SIZE] = {0};
    for (int i = 0; i < STR_LEN; i++) {
        int c = ch2code(s[i]);
        if (c >= 0) Peq[c] |= (1u << i);
    }
    uint32_t VP = ~0u, VN = 0;
    int score = STR_LEN;
    uint32_t mask = (1u << (STR_LEN - 1));
    for (int j = 0; j < STR_LEN; j++) {
        int tc = ch2code(t[j]);
        if (tc < 0) return 0;
        uint32_t X = Peq[tc] | VN;
        uint32_t D0 = (((X & VP) + VP) ^ VP) | X;
        uint32_t HP = VN | ~(D0 | VP);
        uint32_t HN = D0 & VP;
        if (HP & mask) score++;
        else if (HN & mask) score--;
        uint32_t HPsh = (HP << 1) | 1u;
        uint32_t HNsh = (HN << 1);
        VP = HNsh | ~(D0 | HPsh);
        VN = HPsh & D0;
    }
    return (score <= 3);
}

int main(int argc, char **argv) {
    if (argc < 3) return 1;

    FILE *in = fopen(argv[2], "rb");
    if (!in) { fprintf(stderr, "[Error] Failed to open index\n"); return 1; }

    int N;
    if (fread(&N, sizeof(int), 1, in) != 1) return 1;
    char (*db)[STR_LEN + 1] = malloc((size_t)N * (STR_LEN + 1));
    fread(db, STR_LEN + 1, (size_t)N, in);
    int offset[MAX_G + 1];
    fread(offset, sizeof(int), MAX_G + 1, in);
    int total_posts;
    fread(&total_posts, sizeof(int), 1, in);
    int *postings = malloc((size_t)total_posts * sizeof(int));
    fread(postings, sizeof(int), (size_t)total_posts, in);
    fclose(in);

    FILE *fq = fopen(argv[1], "r");
    if (!fq) { fprintf(stderr, "[Error] Failed to open query\n"); return 1; }

    uint8_t *cnt = calloc((size_t)N, sizeof(uint8_t));
    int *touched = malloc((size_t)N * sizeof(int));
    char buf[256];
    int q_count = 0;

    // 最初にクエリの総行数を数える（進捗表示のため）
    int total_queries = 0;
    while (fgets(buf, sizeof(buf), fq)) total_queries++;
    rewind(fq);

    fprintf(stderr, "[Start] Processing %d queries...\n", total_queries);

    while (fgets(buf, sizeof(buf), fq)) {
        buf[strcspn(buf, "\r\n")] = '\0';
        if (strlen(buf) != STR_LEN) continue;

        int touched_len = 0;
        for (int p = 0; p <= STR_LEN - QGRAM; p++) {
            int g = qgram_id(buf, p);
            if (g < 0) continue;
            for (int k = offset[g]; k < offset[g+1]; k++) {
                int id = postings[k];
                if (cnt[id] == 0) touched[touched_len++] = id;
                if (cnt[id] < 255) cnt[id]++;
            }
        }

        int found = 0;
        for (int i = 0; i < touched_len; i++) {
            int id = touched[i];
            if (cnt[id] >= 4) {
                if (edit_distance_le3(db[id], buf)) {
                    found = 1;
                    break;
                }
            }
        }
        putchar(found ? '1' : '0');
        for (int i = 0; i < touched_len; i++) cnt[touched[i]] = 0;

        q_count++;
        // 10,000件ごと進捗を表示
        if (q_count % 10000 == 0) {
            fprintf(stderr, "[Progress] %d / %d queries finished (%.1f%%)\n", 
                    q_count, total_queries, (float)q_count / total_queries * 100);
        }
    }
    putchar('\n');

    fclose(fq);
    fprintf(stderr, "[Success] All queries processed.\n");
    free(db); free(postings); free(cnt); free(touched);
    return 0;
}
