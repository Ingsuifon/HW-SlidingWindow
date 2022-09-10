#include <algorithm>
#include <array>
#include <charconv>
#include <fstream>
#include <iostream>
#include <queue>
#include <vector>

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
        ifile.read(reinterpret_cast<char*>(data[i][j].data()), block_size << 1);
      }
    }
    ifile.close();
  }

  void compress() {
    std::deque<uint16_t> horizontal_window;
    std::deque<uint16_t> vertical_window;

    /* Process the data of Time_0. Because it has no precedent,
     * it uses more information of locality.
     */
    for (int j = 0; j < dim[1]; ++j) {
      // Copy the data of the first longtitude.
      std::copy_n(data[0][j].begin(), dim[3], result[0][j].begin());
      // Initiate the horizontal_window with 9 NULL values as first half.
      for (int cnt = 0; cnt < 9; ++cnt)
        horizontal_window.emplace_back(INT16_MIN);
      for (int cnt = 0; cnt < 7; ++cnt)
        horizontal_window.emplace_back(data[0][j][cnt]);

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
        delta = ((int16_t)delta >> 15) ^ (delta << 1);  // ZigZag
        result[0][j][k] = delta;
        int index = it - horizontal_window.begin();
        encode_index1(index);
      }
      horizontal_window.clear();
    }

    //Process the data of other data.
    for (int i = 1; i < dim[0]; ++i) {
      for (int j = 0; j < dim[1]; ++j) {
        
        for (int k = 0; k < block_size; ++k) {
        }
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

  void encode_index1(int index) {
    index -= 8;
    if (size & 1 == 0)
      code.emplace_back(index & 15);
    else
      code.back() ^= (index << 4);
    ++size;
  }

  std::string input;
  std::string output;
  uint32_t dim[4], block_size;
  std::vector<std::vector<std::vector<uint16_t>>> data, result;

  std::vector<int8_t> code{0};
  uint32_t size;
};