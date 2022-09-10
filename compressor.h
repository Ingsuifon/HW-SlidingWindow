#include <charconv>
#include <fstream>
#include <iostream>
#include <vector>

class Compressor {
 public:
  Compressor(std::string in, std::string out) : input{in}, output{out} {}
  void compress() {
    read_file();
  }

 private:
  void read_file() {
    size_t end { input.find(".raw") };
    size_t begin { end };

    while (std::isdigit(input[begin - 1]) || input[begin - 1] == 'X')
      --begin;

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

    for (int i = 0; i < 4; i++)
        std::cout << dim[i] << std::endl;
  }

  std::string input;
  std::string output;
  uint32_t dim[4];
};