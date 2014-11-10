
Serialization tests
-------------------

The serialization tests are a set of files that document the serialization of various data structures,
and code to test that the results of the serialization functionality match the documentation.

To add more tests, do the following:

- Create `tests/drltc_tests/serial/serial_test_something.cpp` following boilerplate in `tests/drltc_tests/serial/serial_test_ecc.cpp`
- Add your new `.cpp` file(s) to the build in `tests/drltc_tests/serial/CMakeLists.txt`
- Add your new function(s) to `tests/drltc_tests/serial/include/serial_tests.hpp`
- Call your new function(s) in `main()` of `tests/drltc_tests/serial/serial.cpp`
- OpenSSL will use deterministic RNG based on the test name
- Call `tester("my_object", my_object)` for each serializable object instantiated by your test
- When the `tester` object is destroyed at the end of function, the destructor will print results for all serializations
- When unknown serializations are found, they are placed in file `unknown-serial.txt`
- To make the serial test result show as `ok` instead of `unknown`, you can simply copy-paste from `unknown-serial.txt` into `tests/drltc_tests/serial/results/something.txt`
- Line breaks, and comments with pound sign, can be added to serial test files to document the wire format

