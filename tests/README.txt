
Current test in parser_test.cpp works by compairing the output of the parser to a known-good output that was previously created.

To create these known-good output files (e.g. after modifying the parser such that the output is expected to change) do this:

1) checkout and create a desktop build of jst
    cd jst
    git checkout topic/tiny_rdk_top
    mkdir build
    cd build
    cmake -DBUILD_TESTING=ON -DTEST_COMCAST_WEBUI=ON ..
    make
    make install

2) cd jst/tests/parser
    ../create_parsed_files.sh

3) git add tests/parser/* tests/parser/include/*


To run the tests:

  cd jst/build/tests/parser
  ../parser_test

To run webui tests:

  cd jst/build/tests/webui
  ../parser_test

