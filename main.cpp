#include <cmath>
#include <iostream>
#include <chrono>
#include <vector>
#include <set>
#include <algorithm>

namespace {
    // min size of stride in bytes
    const int MIN_STRIDE = 64;
    // max size of stride in bytes
    const int MAX_STRIDE = 512 * 1024;
    const int MAX_ASSOC = 20;
    const int COUNT_OF_STRIDES = 14;

    int eval_stride_by_num(int stride_num) {
        return static_cast<int>(pow(2, static_cast<double>(stride_num + 6)));
    }

    /* Time measuring begin
     * See https://homepages.cwi.nl/~manegold/Calibrator/#2
     */
    // Best access time at specified number of spots and stride. Units of measure: 10^(-11) seconds
    long timings[MAX_ASSOC][COUNT_OF_STRIDES];
    const int MIN_TIME = 10000;
    const int NUM_LOADS = 100000;

    char *array = (char *) malloc(MAX_ASSOC * MAX_STRIDE * 16);

    long use_result_dummy;    /* !static for optimizers. */
    void use_pointer(void *result) { use_result_dummy += (long) result; }

    long measure_time(int cur_stride, int cur_spots) {
        long range = cur_stride * cur_spots;
        char **p;
        long i, j = 1, tries;
        long time, best = std::numeric_limits<long>::max();

        i = range - cur_stride;
        for (; i >= 0; i -= cur_stride) {
            char *next;

            p = (char **) &array[i];
            if (i < cur_stride) {
                next = &array[range - cur_stride];
            } else {
                next = &array[i - cur_stride];
            }
            *p = next;
        }

#define    ONE     p = (char **)*p;
#define    TEN     ONE ONE ONE ONE ONE ONE ONE ONE ONE ONE
#define    HUNDRED     TEN TEN TEN TEN TEN TEN TEN TEN TEN TEN

        for (tries = 0; tries < 20; ++tries) {
            i = (j * NUM_LOADS);
            auto start = std::chrono::high_resolution_clock::now();
            while (i > 0) {
                HUNDRED
                i -= 100;
            }
            auto end = std::chrono::high_resolution_clock::now();
            time = (end - start).count();
            use_pointer((void *) p);
            if (time <= MIN_TIME) {
                j *= 2;
                tries--;
            } else {
                time /= j;
                if (time < best) {
                    best = time;
                }
            }
        }
        return best / 1000;
    }

    /* Time measuring end
     */

    bool jumps[MAX_ASSOC][COUNT_OF_STRIDES];

    bool check_jump(long cur_time, long prev_time) {
        return std::fabs(cur_time - prev_time) > 50;
    }

    void record_jump(int cur_stride, int cur_spots) {
        jumps[cur_spots][cur_stride] = true;
    }

    int no_movements_stride_idx = COUNT_OF_STRIDES - 1;
    // cache size & assoc in bytes
    std::vector<std::pair<int, int>> entities;

    void detect_entities() {
        std::set<int> no_movements_jumps;
        for (int i = 0; i < MAX_ASSOC; ++i) {
            if (jumps[i][no_movements_stride_idx]) {
                no_movements_jumps.insert(i);
            }
        }
        for (int i = no_movements_stride_idx; i >= 0; --i) {
            std::set<int> founded;
            for (auto &jump: no_movements_jumps) {
                if (!jumps[jump][i]) {
                    entities.emplace_back((jump + 1) * eval_stride_by_num(i + 1), jump + 1);
                    founded.insert(jump);
                }
            }
            for (auto &jump: founded) {
                no_movements_jumps.erase(jump);
            }
        }
        std::sort(entities.begin(), entities.end());
    }

    // line sizes in bytes
    std::vector<int> line_sizes;

    void detect_line_sizes() {
        for (auto &entity: entities) {
            int higher_stride = entity.first / entity.second;
            int prev_first_jump = -1;
            int cur_first_jump = -1;
            long prev_access_time = -1;
            long cur_access_time = -1;
            for (int L = 1; L <= higher_stride; L <<= 1) {
                for (int spots = 1; spots <= 512; spots <<= 1) {
                    cur_access_time = measure_time(higher_stride + L, spots);
                    if (prev_access_time != -1 && check_jump(cur_access_time, prev_access_time)) {
                        cur_first_jump = spots;
                        break;
                    }
                    prev_access_time = cur_access_time;
                }
                if (prev_first_jump != -1 && cur_first_jump > prev_first_jump) {
                    line_sizes.push_back(L * entity.second);
                }
                prev_first_jump = cur_first_jump;
            }
        }
    }

    void analyze_caches() {
        for (int stride_num = 0; stride_num < COUNT_OF_STRIDES; ++stride_num) {
            timings[0][stride_num] = measure_time(eval_stride_by_num(stride_num), 1);
            for (int spots = 1; spots < MAX_ASSOC; ++spots) {
                timings[spots][stride_num] = measure_time(eval_stride_by_num(stride_num), spots + 1);
                bool jump = check_jump(timings[spots][stride_num], timings[spots - 1][stride_num]);
                if (jump) {
                    record_jump(stride_num, spots - 1);
                }
            }
        }
        detect_entities();
        detect_line_sizes();
    }

    void print_timings() {
        std::cout << "STRIDE  ";
        for (int i = 0; i < COUNT_OF_STRIDES; ++i) {
            std::cout << eval_stride_by_num(i) << "  ";
        }
        std::cout << std::endl;
        std::cout << "SPOTS" << std::endl;
        for (int i = 0; i < MAX_ASSOC; ++i) {
            std::cout << "  " << i + 1 << ((i < 9) ? "\t" : "");
            for (int j = 0; j < COUNT_OF_STRIDES; ++j) {
                std::cout << "  " << timings[i][j] << " ";
            }
            std::cout << std::endl;
        }
    }
}

int main() {
    analyze_caches();

    // print_timings();
    for (int i = 0; i < entities.size(); ++i) {
        std::cout << "L" << i + 1 << " cache size: " << entities[i].first << ", assoc: " << entities[i].second
                  << ", cache line: " << line_sizes[i] << std::endl;
    }
    return 0;
}
