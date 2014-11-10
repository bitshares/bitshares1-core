
#pragma once

#include <iostream>
#include <map>
#include <set>
#include <vector>

#include <fc/optional.hpp>
#include <fc/shared_ptr.hpp>
#include <fc/string.hpp>
#include <fc/io/iostream.hpp>
#include <fc/io/raw.hpp>
#include <fc/io/sstream.hpp>

#define SERIAL_TEST_STATUS_UNTESTED 1
#define SERIAL_TEST_STATUS_UNKNOWN  2
#define SERIAL_TEST_STATUS_FAILED   3
#define SERIAL_TEST_STATUS_PASSED   4

class test_result
{
public:
    test_result(
        fc::string _name,
        fc::optional<fc::string> _expected_result,
        fc::optional<fc::string> _actual_result)
        : name(_name),
          expected_result(_expected_result),
          actual_result(_actual_result)
    {
        return;
    }
    
    fc::string name;
    fc::optional<fc::string> expected_result;
    fc::optional<fc::string> actual_result;
};

class serial_tester
{
public:
	serial_tester(fc::string& _test_name, fc::ostream* _unknown_output)
        : test_name(_test_name),
          unknown_output(_unknown_output)
    {
        return;
    }
    
    virtual ~serial_tester();

	void add_expected_results(fc::istream &infile);
    void add_test_result(fc::string& name, fc::string& value);
    
    template<typename T> serial_tester& operator()(fc::string& name, T& value)
    {
        fc::stringstream ss;
        fc::string str_data;
        fc::raw::pack(ss, value);
        str_data = ss.str();
        this->add_test_result(name, str_data);
        return *this;
    }
    
    template<typename T> serial_tester& operator()(const char* name, T& value)
    {
        fc::string s(name);
        return (*this)(s, value);
    }
	
	static void print_hex_char(fc::ostream& out, char c);
	static void print_hex_string(fc::ostream& out, fc::string &s, int indent=0);
	static void print_hex_char(char c);
	static void print_hex_string(fc::string &s, int indent=0);

    void process_results();
    void print_results();

    fc::string test_name;

    std::vector<std::pair<fc::string, fc::string>> v_expected_results;
	std::map<fc::string, fc::string> m_expected_results;
	std::set<fc::string> s_untested_cases;
    
    std::vector<test_result> v_test_results;
    
    fc::ostream* unknown_output;
};
