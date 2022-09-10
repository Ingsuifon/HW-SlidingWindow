#include "compressor.h"

#include <iostream>

int main(int argc, char* argv[]) {
  char code = *argv[1];
  std::string input{argv[2]}, output{argv[3]};

  Compressor c(input, output);
  c.compress();
  return 0;
}