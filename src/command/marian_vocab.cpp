#include "marian.h"

#include "3rd_party/cxxopts.hpp"
#include "common/logging.h"
#include "data/vocab.h"

int main(int argc, char** argv) {
  using namespace marian;

  createLoggers();

  cxxopts::Options desc(argv[0]);
  // clang-format off
  desc.add_options()
    ("m,max-size", "Generate only  arg  most common vocabulary items",
     cxxopts::value<size_t>()->default_value("0"))
    ("h,help", "Print this message and exit")
    ;
  // clang-format on

  size_t maxSize = 0;

  try {
    auto vm = desc.parse(argc, argv);
    if(vm.count("help")) {
      std::cerr << desc.help();
      exit(0);
    }
    maxSize = vm["max-size"].as<size_t>();

  } catch(cxxopts::OptionException& e) {
    std::cerr << "Error: " << e.what() << std::endl << std::endl;
    std::cerr << desc.help();
    exit(1);
  }

  LOG(info, "Creating vocabulary...");

  auto vocab = New<Vocab>();
  InputFileStream corpusStrm(std::cin);
  OutputFileStream vocabStrm(std::cout);
  vocab->create(corpusStrm, vocabStrm, maxSize);

  LOG(info, "Finished");

  return 0;
}
