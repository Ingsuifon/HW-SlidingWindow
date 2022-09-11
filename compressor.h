#include <algorithm>
#include <array>
#include <charconv>
#include <cstring>
#include <fstream>
#include <iostream>
#include <queue>
#include <vector>

#include "fse/lib/fse.h"
#include "fse/lib/huf.h"

class Compressor {
 public:
  Compressor(std::string in, std::string out) : input{in}, output{out} {
    get_dimension();
    data.resize(dim[0]);
    result.resize(dim[0]);

    for (uint32_t i = 0; i < dim[0]; ++i) {
      data[i].resize(dim[1]);
      result[i].resize(dim[1]);
      for (uint32_t j = 0; j < dim[1]; ++j) {
        data[i][j].resize(block_size);
        result[i][j].resize(block_size);
      }
    }

    std::ifstream ifile(input);
    for (uint32_t i = 0; i < dim[0]; ++i) {
      for (uint32_t j = 0; j < dim[1]; ++j) {
        ifile.read(reinterpret_cast<char *>(data[i][j].data()),
                   block_size << 1);
      }
    }
    ifile.close();
  }

  void compress() {
    std::deque<uint16_t> horizontal_window;
    std::deque<uint16_t> vertical_window;
    int cnt;
    /*
     * Process the data of Time_0. Because it has no precedent,
     * it uses more information of locality.
     */
    for (int j = 0; j < dim[1]; ++j) {
      // Copy the data of the first longtitude.
      std::copy_n(data[0][j].begin(), dim[3], result[0][j].begin());
      // Initiate the horizontal_window with 9 NULL values as first half.
      cnt = 0;
      while (cnt++ < 9) horizontal_window.emplace_back(0);
      cnt = 0;
      while (cnt++ < 7) horizontal_window.emplace_back(data[0][j][cnt]);

      for (int k = dim[3]; k < block_size; ++k) {
        // Move foreward.
        horizontal_window.pop_front();
        horizontal_window.emplace_back(k - dim[3] + 7);
        // Find the point which has the closest value to the current point.
        auto it = std::min_element(
            k - dim[3] > 7 ? horizontal_window.begin()
                           : horizontal_window.begin() + 8 - (k - dim[3]),
            horizontal_window.end(), [&](uint16_t a, uint16_t b) {
              return (data[0][j][k] ^ a) < (data[0][j][k] ^ b);
            });
        uint16_t delta = data[0][j][k] ^ *it;
        // delta = ((int16_t)delta >> 15) ^ (delta << 1);  // ZigZag
        result[0][j][k] = delta;
        int index = it - horizontal_window.begin();
        encode_index(index);
      }
      horizontal_window.clear();
    }
    // horizontal_window.clear();

    // Process rest data.
    for (int i = 1; i < dim[0]; ++i) {
      for (int j = 0; j < dim[1]; ++j) {
        cnt = 0;
        while (cnt++ < 5) {
          vertical_window.emplace_back(0);
          horizontal_window.emplace_back(0);
        }
        cnt = 0;
        while (cnt++ < 3) {
          vertical_window.emplace_back(data[i - 1][j][cnt]);
          horizontal_window.emplace_back(data[i][j][cnt]);
        }

        for (int k = 0; k < block_size; ++k) {
          // Move foreward.
          vertical_window.pop_front();
          vertical_window.emplace_back(data[i - 1][j][k + 3]);
          if (k >= dim[3]) {
            horizontal_window.pop_front();
            horizontal_window.emplace_back(data[i][j][k + 3]);
          }

          auto it_ver = std::min(
              k > 3 ? vertical_window.begin() : vertical_window.begin() + 4 - k,
              vertical_window.end(), [&](uint16_t a, uint16_t b) {
                return (data[i][j][k] ^ a) < (data[i][j][k] ^ b);
              });

          if (k >= dim[3]) {
            auto it_hor = std::min(
                k - dim[3] > 3 ? horizontal_window.begin()
                               : horizontal_window.begin() + 4 - (k - dim[3]),
                horizontal_window.end(), [&](uint16_t a, uint32_t b) {
                  return (data[i][j][k] ^ a) < (data[i][j][k] ^ b);
                });
            uint16_t delta1 = data[i][j][k] ^ *it_ver;
            uint16_t delta2 = data[i][j][k] ^ *it_hor;
            result[i][j][k] = delta1 < delta2 ? delta1 : delta2;
            int index = delta1 < delta2
                            ? it_ver - vertical_window.begin()
                            : it_hor - horizontal_window.begin() + 8;
          } else {
            result[i][j][k] = data[i][j][k] ^ *it_ver;
            int index = it_ver - vertical_window.begin();
            encode_index(index);
          }
        }
        vertical_window.clear();
        horizontal_window.clear();
      }
    }
  }

 private:
  void get_dimension() {
    size_t end{input.find(".raw")};
    size_t begin{end};

    while (std::isdigit(input[begin - 1]) || input[begin - 1] == 'X') --begin;

    std::string sub_str{input.substr(begin, end - begin)};

    uint32_t res = 0, tmp;
    size_t idx;
    while ((idx = sub_str.find('X')) != std::string::npos) {
      std::string str_tmp = sub_str.substr(0, idx);
      std::from_chars(str_tmp.c_str(), str_tmp.c_str() + str_tmp.length(), tmp,
                      10);
      dim[res++] = tmp;
      sub_str = sub_str.substr(idx + 1);
    }

    std::from_chars(sub_str.c_str(), sub_str.c_str() + sub_str.length(), tmp,
                    10);
    dim[res] = tmp;
    block_size = dim[2] * dim[3];

    for (int i = 0; i < 4; i++) std::cout << dim[i] << std::endl;
  }

  inline void encode_index(int index) {
    if (size & 1 == 0)
      code.emplace_back(index & 15);
    else
      code.back() ^= (index << 4);
    size ^= 1;
  }

  static int entropy_compress(void *src, int src_size, void *dst,
                              int dst_size) {
    int compressed_size = FSE_compress(dst, dst_size, src, src_size);

    if (compressed_size == 0 || compressed_size == 1 ||
        FSE_isError(compressed_size))
      return 0;  // RLE not handled;
    return compressed_size;
  }

  typedef struct {
    uint64_t *data;
    uint32_t pos, slot;
  } vli_struct;

  static vli_struct *init_vli_stream(int size) {
    vli_struct *vli = (vli_struct *)malloc(sizeof(vli_struct));
    vli->data = (uint64_t *)malloc(size);
    memset(vli->data, 0, size);
    vli->pos = 0;
    vli->slot = 64;

    return vli;
  }

  static void set_stream(uint16_t length, uint16_t value, vli_struct *vli) {
    if (vli->slot >= length) {
      vli->data[vli->pos] = (vli->data[vli->pos] << length) | value;
      vli->slot -= length;
    } else {
      uint16_t sub_len = length - vli->slot;
      vli->data[vli->pos] =
          (vli->data[vli->pos] << vli->slot) | (value >> sub_len);
      vli->pos++;
      vli->slot = 64;
      set_stream(sub_len, value & (0xffff >> (16 - sub_len)), vli);
    }
  }

  int vli_compress(void *src, int src_size, void *dst, int dst_size) {
    vli_struct *val_vli = init_vli_stream(src_size * sizeof(uint16_t)),
               *len_vli = init_vli_stream(src_size * sizeof(uint16_t) >> 1);

    uint16_t *src_ptr = (uint16_t *)src;
    for (int i = 0; i < src_size; i++) {
      uint16_t tmp_val = src_ptr[i];
      uint16_t mask = 0x8000, tmp_len = 16;
      for (int j = 0; j < 16; j++) {
        if (mask & tmp_val) break;
        tmp_len--;
        mask >>= 1;
      }
      if (tmp_len != 0) tmp_len--;
      set_stream(4, tmp_len, len_vli);
      set_stream(tmp_len + 1, tmp_val, val_vli);
    }
    len_vli->data[len_vli->pos] <<= len_vli->slot;
    val_vli->data[val_vli->pos] <<= val_vli->slot;

    int len_size = (len_vli->pos + 1) * sizeof(uint64_t),
        val_szie = (val_vli->pos + 1) * sizeof(uint64_t);

    int cmpd_len_size =
        entropy_compress(len_vli->data, len_size, (uint8_t *)dst + sizeof(int),
                         dst_size * sizeof(uint16_t) - sizeof(int));
    if (cmpd_len_size <= 0) {
      cmpd_len_size = len_size;
      memcpy(dst + sizeof(int), len_vli->data, cmpd_len_size);
    }
    *((int *)dst) = cmpd_len_size;
    uint8_t *dst_ptr = (uint8_t *)dst + sizeof(int) + cmpd_len_size;

    int cmpd_val_size = entropy_compress(
        val_vli->data, val_szie, dst_ptr + sizeof(int),
        dst_size * sizeof(uint16_t) - cmpd_len_size - sizeof(int) * 2);
    if (cmpd_val_size <= 0) {
      cmpd_val_size = val_szie;
      memcpy(dst_ptr + sizeof(int), val_vli->data, cmpd_val_size);
    }
    *((int *)dst_ptr) = cmpd_val_size;

    free(val_vli->data);
    free(val_vli);
    free(len_vli->data);
    free(len_vli);

    return cmpd_len_size + cmpd_val_size + sizeof(int) * 2;
  }

  std::string input;
  std::string output;
  uint32_t dim[4], block_size;
  std::vector<std::vector<std::vector<uint16_t>>> data, result;

  std::vector<int8_t> code{0};
  uint8_t size;
};